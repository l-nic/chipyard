#include <stdio.h>
#include <stdlib.h>
#include "lnic.h"

#define USE_MICA 0

#define CHAINREP_FLAGS_FROM_TESTER    (1 << 7)
#define CHAINREP_FLAGS_OP_READ        (1 << 6)
#define CHAINREP_FLAGS_OP_WRITE       (1 << 5)

#define CLIENT_IP 0x0a000002
#define CLIENT_CONTEXT 0
#define SERVER_CONTEXT 1


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
#else
uint64_t test_kv[32];
#endif

int run_client(int cid) {
  uint64_t app_hdr;

  printf("[%d] client waiting.\n", cid);
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

#define CHAIN_SIZE 3
  uint32_t node_ips[] = {CLIENT_IP+1, CLIENT_IP+2, CLIENT_IP+3};
  uint8_t node_ctxs[] = {SERVER_CONTEXT, SERVER_CONTEXT, SERVER_CONTEXT};

  uint64_t msg_key = 0x1;
  uint64_t msg_val = 0x7;

  uint8_t node_cnt = CHAIN_SIZE - 1;
  uint16_t msg_len = 8 + (node_cnt * 8) + 8 + 8;
  app_hdr = ((uint64_t)node_ips[0] << 32) | (node_ctxs[0] << 16) | msg_len;
  lnic_write_r(app_hdr);
  uint8_t client_ctx = cid;
  uint32_t client_ip = CLIENT_IP;

  uint8_t flags = CHAINREP_FLAGS_OP_WRITE;
  uint8_t seq = 0;
  uint64_t cr_meta_fields = ((uint64_t)flags << 56) | ((uint64_t)seq << 48) | ((uint64_t)node_cnt << 40) | ((uint64_t)client_ctx << 32) | client_ip;
  lnic_write_r(cr_meta_fields);

  for (unsigned i = 1; i < CHAIN_SIZE; i++)
    lnic_write_r(((uint64_t)node_ctxs[i] << 32) | node_ips[i]);

  lnic_write_r(msg_key);
  lnic_write_r(msg_val);

  lnic_wait();
  app_hdr = lnic_read();
  uint32_t src_ip = (app_hdr & IP_MASK) >> 32;
  uint16_t src_context = (app_hdr & CONTEXT_MASK) >> 16;
  uint16_t rx_msg_len = app_hdr & LEN_MASK;
  printf("[%d] --> Received from 0x%x:%d msg of length: %u bytes\n", cid, src_ip, src_context, rx_msg_len);

  cr_meta_fields = lnic_read();
  flags = (uint8_t) (cr_meta_fields >> 56);
  seq = (uint8_t) (cr_meta_fields >> 48);
  node_cnt = (uint8_t) (cr_meta_fields >> 40);
  client_ctx = (uint8_t) (cr_meta_fields >> 32);
  client_ip = (uint32_t) cr_meta_fields;
  for (unsigned i = 0; i < node_cnt; i++)
    lnic_read();
  msg_key = lnic_read();
  msg_val = lnic_read();
  printf("[%d] flags=%x seq=%d node_cnt=%d client_ctx=%d client_ip=%x key=0x%lx val=0x%lx\n", cid, flags, seq, node_cnt, client_ctx, client_ip, msg_key, msg_val);

  return EXIT_SUCCESS;
}

int run_server(int core_id) {
  uint64_t app_hdr;
  uint64_t cr_meta_fields;
  uint32_t client_ip;
  uint8_t client_ctx;
  uint8_t flags;
  uint8_t seq;
  uint8_t node_cnt;
  uint64_t msg_key;
  uint64_t msg_val;
	uint16_t msg_len;
  uint64_t nodes[4];
#if USE_MICA
  uint64_t key_hash;
  MicaResult out_result;
  FixedTable::ft_key_t ft_key;
  FixedTable table(kValSize, core_id);
#endif

  unsigned last_seq = 0;

  printf("[%d] server ready.\n", core_id);

  arch_spin_lock(&up_lock);
  server_up = true;
  arch_spin_unlock(&up_lock);

	while (1) {
		lnic_wait();
		app_hdr = lnic_read();
		msg_len = (uint16_t)app_hdr;
		printf("[%d] --> Received msg of length: %u bytes\n", core_id, msg_len);

    cr_meta_fields = lnic_read();
    flags = (uint8_t) (cr_meta_fields >> 56);
    seq = (uint8_t) (cr_meta_fields >> 48);
    node_cnt = (uint8_t) (cr_meta_fields >> 40);
    client_ctx = (uint8_t) (cr_meta_fields >> 32);
    client_ip = (uint32_t) cr_meta_fields;
    for (unsigned i = 0; i < node_cnt; i++) {
      nodes[i] = lnic_read();
    }
    msg_key = lnic_read();
    msg_val = lnic_read();

    unsigned new_node_head = 0;
    uint32_t dst_ip = 0;
    uint16_t dst_context = 0;

#if USE_MICA
    key_hash = mica_hash(&msg_key, sizeof(msg_key));
    ft_key.qword[0] = msg_key;
#endif
    printf("[%d] type=%s, seq=%d, node_cnt=%d\n", core_id, flags & CHAINREP_FLAGS_OP_WRITE ? "W" : "R", seq, node_cnt);

    if (flags & CHAINREP_FLAGS_OP_READ) {
#if USE_MICA
      out_result = table.get(key_hash, ft_key, reinterpret_cast<char *>(&msg_val));
      if (out_result != MicaResult::kSuccess) {
        printf("[%d] GET failed for key %lu.\n", core_id, msg_key);
      }
#else
      msg_val = test_kv[msg_key];
#endif
      dst_ip = client_ip;
      printf("[%d] GET key=%lu val=%lu\n", core_id, msg_key, msg_val);
    }
    else {
      (void)last_seq;
      // TODO: uncomment this to drop out-of-order:
      //if (seq < last_seq) continue; // drop packet
      last_seq = seq;
      if (node_cnt == 0) { // we are at the tail
        dst_ip = client_ip;
        dst_context = client_ctx;
      }
      else {
        dst_ip = (uint32_t) nodes[0];
        dst_context = nodes[0] >> 32;
        new_node_head = 1;
        node_cnt -= 1;
      }
#if USE_MICA
      out_result = table.set(key_hash, ft_key, reinterpret_cast<char *>(&msg_val));
      if (out_result != MicaResult::kSuccess) {
        printf("[%d] Inserting key %lu failed.\n", core_id, msg_key);
      }
#else
      test_kv[msg_key] = msg_val;
#endif
      printf("[%d] WRITE key=%lu val=%lu\n", core_id, msg_key, msg_val);
    }

    unsigned msg_len = 8 + (node_cnt * 8) + 8 + 8;
    app_hdr = ((uint64_t)dst_ip << 32) | (dst_context << 16) | msg_len;
    lnic_write_r(app_hdr);

    flags &= ~CHAINREP_FLAGS_FROM_TESTER;
    cr_meta_fields = ((uint64_t)flags << 56) | ((uint64_t)seq << 48) | ((uint64_t)node_cnt << 40) | ((uint64_t)client_ctx << 32) | client_ip;
    lnic_write_r(cr_meta_fields);

    for (unsigned i = new_node_head; i < new_node_head+node_cnt; i++) {
      lnic_write_r(nodes[i]);
    }

    lnic_write_r(msg_key);
    lnic_write_r(msg_val);

    lnic_msg_done();
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

  uint64_t priority = 0;
  lnic_add_context(cid, priority);

  // wait for all cores to boot -- TODO: is there a better way than this?
  for (int i = 0; i < 1000; i++) {
    asm volatile("nop");
  }

  int ret;
  if (nic_ip_addr == CLIENT_IP && cid == 0)
    ret = run_client(cid);
  else if (cid == 1)
    ret = run_server(cid);
  else
    ret = EXIT_SUCCESS;

  return ret;
}

}
