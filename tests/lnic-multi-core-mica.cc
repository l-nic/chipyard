#include <stdio.h>
#include <stdlib.h>
#include "lnic.h"

#define MICA_R_TYPE 1
#define MICA_W_TYPE 2

#define VALUE_SIZE_WORDS 64
#define KEY_SIZE_WORDS   2

#define SERVER_IP 0x0a000002
#define SERVER_CONTEXT 0

#define CLIENT_IP 0x0a000003
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

static constexpr size_t kValSize = VALUE_SIZE_WORDS * 8;

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

  static constexpr size_t kKeySize = 8 * KEY_SIZE_WORDS;

  static constexpr bool concurrentRead = false;
  static constexpr bool concurrentWrite = false;

  //static constexpr size_t itemCount = 100000;
  static constexpr size_t itemCount = 10;
};

typedef mica::table::FixedTable<MyFixedTableConfig> FixedTable;
typedef mica::table::Result MicaResult;

template <typename T>
static uint64_t mica_hash(const T *key) {
  return ::mica::util::hash(key, 8*KEY_SIZE_WORDS);
}
#endif // USE_MICA

int run_client(int cid) {
	uint64_t app_hdr;
  uint32_t src_ip;
  uint16_t src_context;
  uint16_t msg_len;
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

  uint64_t msg_val[VALUE_SIZE_WORDS];
  uint64_t msg_key[KEY_SIZE_WORDS];
  msg_key[1] = 0;
  msg_val[0] = 0x7;
  uint64_t ack;

#define NUM_ITERS 16
  for (int i = 0; i < NUM_ITERS; i++) {
    msg_key[0] = (i % 4) + 1;
    // SET
    msg_len = 8 + (8 * KEY_SIZE_WORDS) + (8 * VALUE_SIZE_WORDS);
    app_hdr = ((uint64_t)dst_ip << 32) | ((uint32_t)dst_context << 16) | msg_len;
    lnic_write_r(app_hdr);
    lnic_write_i(MICA_W_TYPE);
    lnic_write_r(msg_key[0]);
    lnic_write_r(msg_key[1]);
    lnic_write_r(msg_val[0]);
    lnic_write_r(msg_val[1]);
    lnic_write_r(msg_val[2]);
    lnic_write_r(msg_val[3]);
    lnic_write_r(msg_val[4]);
    lnic_write_r(msg_val[5]);
    lnic_write_r(msg_val[6]);
    lnic_write_r(msg_val[7]);
    lnic_write_r(msg_val[8]);
    lnic_write_r(msg_val[9]);
    lnic_write_r(msg_val[10]);
    lnic_write_r(msg_val[11]);
    lnic_write_r(msg_val[12]);
    lnic_write_r(msg_val[13]);
    lnic_write_r(msg_val[14]);
    lnic_write_r(msg_val[15]);
    lnic_write_r(msg_val[16]);
    lnic_write_r(msg_val[17]);
    lnic_write_r(msg_val[18]);
    lnic_write_r(msg_val[19]);
    lnic_write_r(msg_val[20]);
    lnic_write_r(msg_val[21]);
    lnic_write_r(msg_val[22]);
    lnic_write_r(msg_val[23]);
    lnic_write_r(msg_val[24]);
    lnic_write_r(msg_val[25]);
    lnic_write_r(msg_val[26]);
    lnic_write_r(msg_val[27]);
    lnic_write_r(msg_val[28]);
    lnic_write_r(msg_val[29]);
    lnic_write_r(msg_val[30]);
    lnic_write_r(msg_val[31]);
    lnic_write_r(msg_val[32]);
    lnic_write_r(msg_val[33]);
    lnic_write_r(msg_val[34]);
    lnic_write_r(msg_val[35]);
    lnic_write_r(msg_val[36]);
    lnic_write_r(msg_val[37]);
    lnic_write_r(msg_val[38]);
    lnic_write_r(msg_val[39]);
    lnic_write_r(msg_val[40]);
    lnic_write_r(msg_val[41]);
    lnic_write_r(msg_val[42]);
    lnic_write_r(msg_val[43]);
    lnic_write_r(msg_val[44]);
    lnic_write_r(msg_val[45]);
    lnic_write_r(msg_val[46]);
    lnic_write_r(msg_val[47]);
    lnic_write_r(msg_val[48]);
    lnic_write_r(msg_val[49]);
    lnic_write_r(msg_val[50]);
    lnic_write_r(msg_val[51]);
    lnic_write_r(msg_val[52]);
    lnic_write_r(msg_val[53]);
    lnic_write_r(msg_val[54]);
    lnic_write_r(msg_val[55]);
    lnic_write_r(msg_val[56]);
    lnic_write_r(msg_val[57]);
    lnic_write_r(msg_val[58]);
    lnic_write_r(msg_val[59]);
    lnic_write_r(msg_val[60]);
    lnic_write_r(msg_val[61]);
    lnic_write_r(msg_val[62]);
    lnic_write_r(msg_val[63]);
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
    ack = lnic_read();
    if (ack != 0x1) printf("CLIENT ERROR: Expected: ack = 0x%x, Received: ack = 0x%lx\n", 0x1, ack);
    lnic_msg_done();
    printf("[%d] client SET key=%lx val=%lx. Latency: %ld\n", cid, msg_key[0], msg_val[0], stop_time-start_time);

    // GET
    app_hdr = ((uint64_t)dst_ip << 32) | ((uint32_t)dst_context << 16) | (8 + (8 * KEY_SIZE_WORDS));
    lnic_write_r(app_hdr);
    lnic_write_i(MICA_R_TYPE);
    lnic_write_r(msg_key[0]);
    lnic_write_r(msg_key[1]);
    start_time = rdcycle();
    lnic_wait();
    stop_time = rdcycle();
    app_hdr = lnic_read();
    src_ip = (app_hdr & IP_MASK) >> 32;
    src_context = (app_hdr & CONTEXT_MASK) >> 16;
    rx_msg_len = app_hdr & LEN_MASK;
    if (src_ip != SERVER_IP) printf("CLIENT ERROR: Expected: correct_sender_ip = %x, Received: src_ip = %x\n", SERVER_IP, src_ip);
    if (src_context != SERVER_CONTEXT) printf("CLIENT ERROR: Expected: correct_src_context = %d, Received: src_context = %d\n", SERVER_CONTEXT, src_context);
    if (rx_msg_len != 8*VALUE_SIZE_WORDS) printf("CLIENT ERROR: Expected: msg_len = %d, Received: msg_len = %d\n", 8*VALUE_SIZE_WORDS, rx_msg_len);
    msg_val[0] = lnic_read();
    for (int j = 1; j < VALUE_SIZE_WORDS; j++)
      lnic_read();
    if (msg_val[0] != 0x7) printf("CLIENT ERROR: Expected: msg_val = 0x%x, Received: msg_val[0] = 0x%lx\n", 0x7, msg_val[0]);
    lnic_msg_done();
    printf("[%d] client GET key=%lx val=%lx. Latency: %ld\n", cid, msg_key[0], msg_val[0], stop_time-start_time);
  }

  return EXIT_SUCCESS;
}

int run_server(int cid) {
	uint64_t app_hdr;
  uint16_t msg_len;
  uint64_t msg_type;
  uint64_t msg_key[KEY_SIZE_WORDS];
  uint64_t msg_val[VALUE_SIZE_WORDS];
  uint64_t start_time, stop_time;
  uint64_t t1, t2, t3;
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
    msg_key[0] = lnic_read();
    msg_key[1] = lnic_read();

    if (msg_type == MICA_W_TYPE) {
      msg_val[0] = lnic_read();
      msg_val[1] = lnic_read();
      msg_val[2] = lnic_read();
      msg_val[3] = lnic_read();
      msg_val[4] = lnic_read();
      msg_val[5] = lnic_read();
      msg_val[6] = lnic_read();
      msg_val[7] = lnic_read();
      msg_val[8] = lnic_read();
      msg_val[9] = lnic_read();
      msg_val[10] = lnic_read();
      msg_val[11] = lnic_read();
      msg_val[12] = lnic_read();
      msg_val[13] = lnic_read();
      msg_val[14] = lnic_read();
      msg_val[15] = lnic_read();
      msg_val[16] = lnic_read();
      msg_val[17] = lnic_read();
      msg_val[18] = lnic_read();
      msg_val[19] = lnic_read();
      msg_val[20] = lnic_read();
      msg_val[21] = lnic_read();
      msg_val[22] = lnic_read();
      msg_val[23] = lnic_read();
      msg_val[24] = lnic_read();
      msg_val[25] = lnic_read();
      msg_val[26] = lnic_read();
      msg_val[27] = lnic_read();
      msg_val[28] = lnic_read();
      msg_val[29] = lnic_read();
      msg_val[30] = lnic_read();
      msg_val[31] = lnic_read();
      msg_val[32] = lnic_read();
      msg_val[33] = lnic_read();
      msg_val[34] = lnic_read();
      msg_val[35] = lnic_read();
      msg_val[36] = lnic_read();
      msg_val[37] = lnic_read();
      msg_val[38] = lnic_read();
      msg_val[39] = lnic_read();
      msg_val[40] = lnic_read();
      msg_val[41] = lnic_read();
      msg_val[42] = lnic_read();
      msg_val[43] = lnic_read();
      msg_val[44] = lnic_read();
      msg_val[45] = lnic_read();
      msg_val[46] = lnic_read();
      msg_val[47] = lnic_read();
      msg_val[48] = lnic_read();
      msg_val[49] = lnic_read();
      msg_val[50] = lnic_read();
      msg_val[51] = lnic_read();
      msg_val[52] = lnic_read();
      msg_val[53] = lnic_read();
      msg_val[54] = lnic_read();
      msg_val[55] = lnic_read();
      msg_val[56] = lnic_read();
      msg_val[57] = lnic_read();
      msg_val[58] = lnic_read();
      msg_val[59] = lnic_read();
      msg_val[60] = lnic_read();
      msg_val[61] = lnic_read();
      msg_val[62] = lnic_read();
      msg_val[63] = lnic_read();
    }

    msg_len = msg_type == MICA_R_TYPE ? 8 * VALUE_SIZE_WORDS : 8;
    lnic_write_r((app_hdr & (IP_MASK | CONTEXT_MASK)) | msg_len);

    t1 = rdcycle();
#if USE_MICA
    key_hash = mica_hash(&msg_key);
    ft_key.qword[0] = msg_key[0];
    ft_key.qword[1] = msg_key[1];
#endif // USE_MICA
    t2 = rdcycle();

    if (msg_type == MICA_R_TYPE) {
#if USE_MICA
      out_result = table.get(key_hash, ft_key, reinterpret_cast<char *>(&msg_val[0]));
      if (out_result != MicaResult::kSuccess) {
        printf("[%d] GET failed for key %lu.\n", cid, msg_key[0]);
      }
#endif // USE_MICA
      t3 = rdcycle();
      lnic_write_r(msg_val[0]);
      lnic_write_r(msg_val[1]);
      lnic_write_r(msg_val[2]);
      lnic_write_r(msg_val[3]);
      lnic_write_r(msg_val[4]);
      lnic_write_r(msg_val[5]);
      lnic_write_r(msg_val[6]);
      lnic_write_r(msg_val[7]);
      lnic_write_r(msg_val[8]);
      lnic_write_r(msg_val[9]);
      lnic_write_r(msg_val[10]);
      lnic_write_r(msg_val[11]);
      lnic_write_r(msg_val[12]);
      lnic_write_r(msg_val[13]);
      lnic_write_r(msg_val[14]);
      lnic_write_r(msg_val[15]);
      lnic_write_r(msg_val[16]);
      lnic_write_r(msg_val[17]);
      lnic_write_r(msg_val[18]);
      lnic_write_r(msg_val[19]);
      lnic_write_r(msg_val[20]);
      lnic_write_r(msg_val[21]);
      lnic_write_r(msg_val[22]);
      lnic_write_r(msg_val[23]);
      lnic_write_r(msg_val[24]);
      lnic_write_r(msg_val[25]);
      lnic_write_r(msg_val[26]);
      lnic_write_r(msg_val[27]);
      lnic_write_r(msg_val[28]);
      lnic_write_r(msg_val[29]);
      lnic_write_r(msg_val[30]);
      lnic_write_r(msg_val[31]);
      lnic_write_r(msg_val[32]);
      lnic_write_r(msg_val[33]);
      lnic_write_r(msg_val[34]);
      lnic_write_r(msg_val[35]);
      lnic_write_r(msg_val[36]);
      lnic_write_r(msg_val[37]);
      lnic_write_r(msg_val[38]);
      lnic_write_r(msg_val[39]);
      lnic_write_r(msg_val[40]);
      lnic_write_r(msg_val[41]);
      lnic_write_r(msg_val[42]);
      lnic_write_r(msg_val[43]);
      lnic_write_r(msg_val[44]);
      lnic_write_r(msg_val[45]);
      lnic_write_r(msg_val[46]);
      lnic_write_r(msg_val[47]);
      lnic_write_r(msg_val[48]);
      lnic_write_r(msg_val[49]);
      lnic_write_r(msg_val[50]);
      lnic_write_r(msg_val[51]);
      lnic_write_r(msg_val[52]);
      lnic_write_r(msg_val[53]);
      lnic_write_r(msg_val[54]);
      lnic_write_r(msg_val[55]);
      lnic_write_r(msg_val[56]);
      lnic_write_r(msg_val[57]);
      lnic_write_r(msg_val[58]);
      lnic_write_r(msg_val[59]);
      lnic_write_r(msg_val[60]);
      lnic_write_r(msg_val[61]);
      lnic_write_r(msg_val[62]);
      lnic_write_r(msg_val[63]);
    }
    else {
#if USE_MICA
      out_result = table.set(key_hash, ft_key, reinterpret_cast<char *>(&msg_val[0]));
      if (out_result != MicaResult::kSuccess) {
        printf("[%d] Inserting key %lu failed.\n", cid, msg_key[0]);
      }
#endif // USE_MICA
      t3 = rdcycle();
      lnic_write_i(0x1); // ACK
    }

    lnic_msg_done();
    stop_time = rdcycle();
    printf("[%d] %s key=%lu val=%lu. Hash lat: %ld     MICA latency: %ld     Total latency: %ld\n", cid,
        msg_type == MICA_R_TYPE ? "GET" : "SET", msg_key[0], msg_val[0], t2-t1, t3-t2, stop_time-start_time);
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

  if (argc < 3) {
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

  uint64_t priority = 0;
  lnic_add_context(cid, priority);

  // wait for all cores to boot -- TODO: is there a better way than this?
  for (int i = 0; i < 1000; i++) {
    asm volatile("nop");
  }

  int ret;
  if (nic_ip_addr == CLIENT_IP && cid == CLIENT_CONTEXT)
    ret = run_client(cid);
  else if (cid == SERVER_CONTEXT)
    ret = run_server(cid);
  else
    ret = EXIT_SUCCESS;

  return ret;
}

}
