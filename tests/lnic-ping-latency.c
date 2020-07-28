#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "lnic.h"

#define CLIENT_IP 0x0a000002
#define CLIENT_CONTEXT 0

#define SERVER_IP 0x0a000002
#define SERVER_CONTEXT 1

#define NUM_MSGS 1
#define MSG_LEN_WORDS 1

bool is_single_core() { return false; }

bool server_up = false;

typedef struct {
  volatile unsigned int lock;
} arch_spinlock_t;

#define arch_spin_is_locked(x) ((x)->lock != 0)

static inline void arch_spin_unlock(arch_spinlock_t *lock) {
  asm volatile (
    "amoswap.w.rl x0, x0, %0"
    : "=A" (lock->lock)
    :: "memory"
    );
}

static inline int arch_spin_trylock(arch_spinlock_t* lock) {
  int tmp = 1, busy;
  asm volatile (
    "amoswap.w.aq %0, %2, %1"
    : "=r"(busy), "+A" (lock->lock)
    : "r"(tmp)
    : "memory"
    );
  return !busy;
}

static inline void arch_spin_lock(arch_spinlock_t* lock) {
  while (1) {
    if (arch_spin_is_locked(lock)) {
      continue;
    }
    if (arch_spin_trylock(lock)) {
      break;
    }
  }
}

arch_spinlock_t up_lock;

void print_app_hdr(uint64_t app_hdr) {
  uint64_t ip = (app_hdr & IP_MASK) >> 32;
  uint64_t context = (app_hdr & CONTEXT_MASK) >> 16;
  uint16_t msg_len = app_hdr & LEN_MASK;
  printf("\tip = %lx\n", ip);
  printf("\tcontext = %ld\n", context);
  printf("\tmsg_len = %d\n", msg_len);
}

int run_client() {
  uint64_t app_hdr;
  uint64_t dst_ip;
  uint64_t dst_context;
  uint64_t src_ip;
  uint64_t src_context;
  uint16_t rx_msg_len;
  uint64_t now;
  uint64_t latency;
  int i;
  int n;
  int ret = 0; // return code

  uint64_t timestamps[NUM_MSGS];
  uint64_t latencies[NUM_MSGS];

  dst_ip = 0x0a000002;
  dst_context = SERVER_CONTEXT;

  while (true) {
    arch_spin_lock(&up_lock);
    if (server_up) {
      arch_spin_unlock(&up_lock);
      break;
    } else {
      arch_spin_unlock(&up_lock);
      for (int k = 0; k < 100; k++) {
        asm volatile("nop");
      }
    }
  }

  for (n = 0; n < NUM_MSGS; n++) {
    // Send msg to server
    printf("Client sending to server\n");
    app_hdr = (dst_ip << 32) | (dst_context << 16) | (MSG_LEN_WORDS*8);
    lnic_write_r(app_hdr);
    now = rdcycle();
    lnic_write_r(now);
    for (i = 1; i < MSG_LEN_WORDS; i++) {
      lnic_write_r(i);
    }

    // receive response from server
    lnic_wait();
    app_hdr = lnic_read();
    // Record latency
    now = rdcycle();
    timestamps[n] = now;
    latencies[n] = now - lnic_read();
    for (i = 1; i < MSG_LEN_WORDS; i++) {
      lnic_read();
    }
    // Check src IP
    src_ip = (app_hdr & IP_MASK) >> 32;
    if (src_ip != SERVER_IP) {
        printf("CLIENT ERROR: Expected: correct_sender_ip = %lx, Received: src_ip = %lx\n", SERVER_IP, src_ip);
        // return -1;
    }
    // Check src context
    src_context = (app_hdr & CONTEXT_MASK) >> 16;
    if (src_context != SERVER_CONTEXT) {
        printf("CLIENT ERROR: Expected: correct_src_context = %ld, Received: src_context = %ld\n", SERVER_CONTEXT, src_context);
        // return -1;
    }
    // Check msg length
    rx_msg_len = app_hdr & LEN_MASK;
    if (rx_msg_len != MSG_LEN_WORDS*8) {
        printf("CLIENT ERROR: Expected: msg_len = %d, Received: msg_len = %d\n", MSG_LEN_WORDS*8, rx_msg_len);
        return -1;
    }
    lnic_msg_done();
  }

  // make sure RX queue is empty
  if (read_csr(0x052) != 0) {
    printf("CLIENT ERROR: RX Queue is not empty after processing all msgs!\n");
    app_hdr = lnic_read();
    printf("CLIENT Rx AppHdr:\n");
    print_app_hdr(app_hdr);
    ret = -1;
  }

  // print latency measurements
  printf("time, latency\n");
  for (n = 0; n < NUM_MSGS; n++) {
    printf("%ld, %ld\n", timestamps[n], latencies[n]);
  }

  return ret; 
}
 
int run_server() {
  uint64_t app_hdr;
  uint16_t msg_len;
  int num_words;
  int i;
  int n;

  arch_spin_lock(&up_lock);
  server_up = true;
  arch_spin_unlock(&up_lock);

  for (n = 0; n < NUM_MSGS; n++) {
    // wait for a pkt to arrive
    lnic_wait();
    // read request application hdr
    app_hdr = lnic_read();

//    // Check src IP
//    uint64_t src_ip = (app_hdr & IP_MASK) >> 32;
//    if (src_ip != CLIENT_IP) {
//        printf("SERVER ERROR: Expected: correct_sender_ip = %lx, Received: src_ip = %lx\n", CLIENT_IP, src_ip);
//        // return -1;
//    }
//    // Check src context
//    uint64_t src_context = (app_hdr & CONTEXT_MASK) >> 16;
//    if (src_context != CLIENT_CONTEXT) {
//        printf("SERVER ERROR: Expected: correct_src_context = %ld, Received: src_context = %ld\n", CLIENT_CONTEXT, src_context);
//        // return -1;
//    }
//    // Check msg length
//    uint16_t rx_msg_len = app_hdr & LEN_MASK;
//    if (rx_msg_len != MSG_LEN_WORDS*8) {
//        printf("SERVER ERROR: Expected: msg_len = %d, Received: msg_len = %d\n", MSG_LEN_WORDS*8, rx_msg_len);
//        return -1;
//    }

    // write response application hdr
    lnic_write_r(app_hdr);
    // extract msg_len
    msg_len = (uint16_t)app_hdr;
    num_words = msg_len/LNIC_WORD_SIZE;
    if (msg_len % LNIC_WORD_SIZE != 0) { num_words++; }
    // copy msg words back into network
    for (i = 0; i < num_words; i++) {
      lnic_copy();
    }
    lnic_msg_done();
  }

  // make sure RX queue is empty
  if (read_csr(0x052) != 0) {
    printf("SERVER ERROR: RX Queue is not empty after processing all msgs!\n");
    return -1;
  }

  return 0;
}

int core_main(uint64_t argc, char** argv, int cid, int nc) {
  uint64_t context_id = cid;
  uint64_t priority = 0;

  if (argc != 3) {
      printf("This program requires passing the L-NIC MAC address, followed by the L-NIC IP address.\n");
      return -1;
  }

  char* nic_ip_str = argv[2];
  uint32_t nic_ip_addr_lendian = 0;
  int retval = inet_pton4(nic_ip_str, nic_ip_str + strlen(nic_ip_str), &nic_ip_addr_lendian);

  // Risc-v is little-endian, but we store ip's as big-endian since the NIC works in big-endian
  uint32_t nic_ip_addr = swap32(nic_ip_addr_lendian);
  if (retval != 1 || nic_ip_addr == 0) {
      printf("Supplied NIC IP address is invalid.\n");
      return -1;
  }
  if (nic_ip_addr != SERVER_IP) {
    while(1);
  }

  lnic_add_context(context_id, priority);

  int ret = 0;
  if (cid == CLIENT_CONTEXT) {
    ret = run_client();
  } else if (cid == SERVER_CONTEXT) {
    ret = run_server();
  }
 
  return ret;
}

