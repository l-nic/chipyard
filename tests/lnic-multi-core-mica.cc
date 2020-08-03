#include <stdio.h>
#include <stdlib.h>
#include "lnic.h"

#define MICA_R_TYPE 1
#define MICA_W_TYPE 2

#define SERVER_IP 0x0a000002
#define SERVER_CONTEXT 0

#define CLIENT_CONTEXT 1

#define USE_MICA 1

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

#if USE_MICA

#include "mica/table/fixedtable.h"
#include "mica/util/hash.h"

static constexpr size_t kValSize = 8;

struct MyFixedTableConfig {
  static constexpr size_t kBucketCap = 7;

  // Support concurrent access. The actual concurrent access is enabled by
  // concurrent_read and concurrent_write in the configuration.
  static constexpr bool kConcurrent = false;

  // Be verbose.
  static constexpr bool kVerbose = false;

  // Collect fine-grained statistics accessible via print_stats() and
  // reset_stats().
  static constexpr bool kCollectStats = false;

  static constexpr size_t kKeySize = 8;

  static constexpr bool concurrentRead = false;
  static constexpr bool concurrentWrite = false;

  static constexpr size_t itemCount = 100000;
};

typedef mica::table::FixedTable<MyFixedTableConfig> FixedTable;
typedef mica::table::Result MicaResult;

template <typename T>
static uint64_t mica_hash(const T *key, size_t key_length) {
  return ::mica::util::hash(key, key_length);
}
#endif // USE_MICA

int run_client(int cid) {
	uint64_t app_hdr;
  uint32_t src_ip;
  uint16_t src_context;
  uint16_t rx_msg_len;
  uint64_t start_time, stop_time;

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

  printf("[%d] client starting.\n", cid);

  uint32_t dst_ip = SERVER_IP;
  uint16_t dst_context = SERVER_CONTEXT;

  uint64_t msg_key = 0x1;
  uint64_t msg_val = 0x7;
  uint64_t recv_msg_val;

#define NUM_ITERS 1000
  for (int i = 0; i < NUM_ITERS; i++) {
    // SET
    app_hdr = ((uint64_t)dst_ip << 32) | ((uint32_t)dst_context << 16) | (24);
    lnic_write_r(app_hdr);
    lnic_write_i(MICA_W_TYPE);
    lnic_write_r(msg_key);
    lnic_write_r(msg_val);
    start_time = rdcycle();
    lnic_wait();
    stop_time = rdcycle();
    app_hdr = lnic_read();
    src_ip = (app_hdr & IP_MASK) >> 32;
    src_context = (app_hdr & CONTEXT_MASK) >> 16;
    rx_msg_len = app_hdr & LEN_MASK;
    if (src_ip != SERVER_IP) printf("CLIENT ERROR: Expected: correct_sender_ip = %x, Received: src_ip = %x\n", SERVER_IP, src_ip);
    if (src_context != SERVER_CONTEXT) printf("CLIENT ERROR: Expected: correct_src_context = %d, Received: src_context = %d\n", SERVER_CONTEXT, src_context);
    if (rx_msg_len != 8) printf("CLIENT ERROR: Expected: msg_len = %d, Received: msg_len = %d\n", 8, rx_msg_len);
    recv_msg_val = lnic_read();
    if (recv_msg_val != msg_val) printf("CLIENT ERROR: Expected: msg_val = 0x%lx, Received: msg_val = 0x%lx\n", msg_val, recv_msg_val);
    lnic_msg_done();
    printf("[%d] client SET key=%lx val=%lx. Latency: %ld\n", cid, msg_key, msg_val, stop_time-start_time);

    // GET
    app_hdr = ((uint64_t)dst_ip << 32) | ((uint32_t)dst_context << 16) | (16);
    lnic_write_r(app_hdr);
    lnic_write_i(MICA_R_TYPE);
    lnic_write_r(msg_key);
    start_time = rdcycle();
    lnic_wait();
    stop_time = rdcycle();
    app_hdr = lnic_read();
    src_ip = (app_hdr & IP_MASK) >> 32;
    src_context = (app_hdr & CONTEXT_MASK) >> 16;
    rx_msg_len = app_hdr & LEN_MASK;
    if (src_ip != SERVER_IP) printf("CLIENT ERROR: Expected: correct_sender_ip = %x, Received: src_ip = %x\n", SERVER_IP, src_ip);
    if (src_context != SERVER_CONTEXT) printf("CLIENT ERROR: Expected: correct_src_context = %d, Received: src_context = %d\n", SERVER_CONTEXT, src_context);
    if (rx_msg_len != 8) printf("CLIENT ERROR: Expected: msg_len = %d, Received: msg_len = %d\n", 8, rx_msg_len);
    recv_msg_val = lnic_read();
    if (recv_msg_val != msg_val) printf("CLIENT ERROR: Expected: msg_val = 0x%lx, Received: msg_val = 0x%lx\n", msg_val, recv_msg_val);
    lnic_msg_done();
    printf("[%d] client GET key=%lx val=%lx. Latency: %ld\n", cid, msg_key, recv_msg_val, stop_time-start_time);
  }

  return EXIT_SUCCESS;
}

int run_server(int cid) {
	uint64_t app_hdr;
  uint64_t msg_type;
  uint64_t msg_key;
  uint64_t msg_val;
  uint64_t start_time, stop_time;
#if USE_MICA
  uint64_t key_hash;
  MicaResult out_result;
  FixedTable::ft_key_t ft_key;
  FixedTable table(kValSize, cid);
#endif // USE_MICA

  printf("[%d] Server ready.\n", cid);

  arch_spin_lock(&up_lock);
  server_up = true;
  arch_spin_unlock(&up_lock);


  while (1) {
    lnic_wait();
    start_time = rdcycle();
    app_hdr = lnic_read();
    //printf("[%d] --> Received msg of length: %u bytes\n", cid, (uint16_t)app_hdr);
    msg_type = lnic_read();
    msg_key = lnic_read();

    //printf("[%d] type=%lu, key=%lu\n", cid, msg_type, msg_key);
#if USE_MICA
    key_hash = mica_hash(&msg_key, sizeof(msg_key));
    ft_key.qword[0] = msg_key;
#endif // USE_MICA

    if (msg_type == MICA_R_TYPE) {
#if USE_MICA
      out_result = table.get(key_hash, ft_key, reinterpret_cast<char *>(&msg_val));
      if (out_result != MicaResult::kSuccess) {
        printf("[%d] GET failed for key %lu.\n", cid, msg_key);
      }
#endif // USE_MICA
    }
    else {
      msg_val = lnic_read();
#if USE_MICA
      out_result = table.set(key_hash, ft_key, reinterpret_cast<char *>(&msg_val));
      if (out_result != MicaResult::kSuccess) {
        printf("[%d] Inserting key %lu failed.\n", cid, msg_key);
      }
#endif // USE_MICA
    }

    lnic_write_r((app_hdr & (IP_MASK | CONTEXT_MASK)) | 8);
    lnic_write_r(msg_val);
    lnic_msg_done();
    stop_time = rdcycle();
    printf("[%d] %s key=%lu val=%lu. Latency: %ld\n", cid,
        msg_type == MICA_R_TYPE ? "GET" : "SET", msg_key, msg_val, stop_time-start_time);
	}

  return EXIT_SUCCESS;
}

extern "C" {

#include <string.h>

// These are defined in syscalls.c
int inet_pton4(const char *src, const char *end, unsigned char *dst);
uint32_t swap32(uint32_t in);

bool is_single_core() { return false; }

int core_main(int argc, char** argv, int cid, int nc) {
  (void)nc;
  printf("args: ");
  for (int i = 1; i < argc; i++) {
    printf("%s ", argv[i]);
  }
  printf("\n");

  if (argc != 3) {
      printf("This program requires passing the L-NIC MAC address, followed by the L-NIC IP address.\n");
      return -1;
  }

  char* nic_ip_str = argv[2];
  uint32_t nic_ip_addr_lendian = 0;
  int retval = inet_pton4(nic_ip_str, nic_ip_str + strlen(nic_ip_str), (unsigned char *)&nic_ip_addr_lendian);

  // Risc-v is little-endian, but we store ip's as big-endian since the NIC works in big-endian
  uint32_t nic_ip_addr = swap32(nic_ip_addr_lendian);
  if (retval != 1 || nic_ip_addr == 0) {
      printf("Supplied NIC IP address is invalid.\n");
      return -1;
  }
  if (nic_ip_addr != SERVER_IP) {
    while(1);
  }

  uint64_t priority = 0;
  lnic_add_context(cid, priority);

  // wait for all cores to boot -- TODO: is there a better way than this?
  for (int i = 0; i < 1000; i++) {
    asm volatile("nop");
  }

  int ret = EXIT_SUCCESS;
  if (cid == CLIENT_CONTEXT) {
    ret = run_client(cid);
  } else if (cid == SERVER_CONTEXT) {
    ret = run_server(cid);
  }

  return ret;
}

}
