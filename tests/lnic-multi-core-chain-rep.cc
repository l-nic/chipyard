#include <stdio.h>
#include <stdlib.h>
#include "lnic.h"

#define USE_MICA 0

#define CHAINREP_OP_READ  1
#define CHAINREP_OP_WRITE 2

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

int process_msgs(int core_id) {
	uint64_t app_hdr;
  uint64_t cr_meta_fields;
  uint32_t client_ip;
  uint8_t flags;
  uint8_t op_type;
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

  printf("[%d] ready.\n", core_id);

	while (1) {
		lnic_wait();
		app_hdr = lnic_read();
		msg_len = (uint16_t)app_hdr;
		printf("[%d] --> Received msg of length: %u bytes\n", core_id, msg_len);

    cr_meta_fields = lnic_read();
    flags = (uint8_t) (cr_meta_fields >> 56);
    op_type = (uint8_t) (cr_meta_fields >> 48);
    seq = (uint16_t) (cr_meta_fields >> 40);
    node_cnt = (uint16_t) (cr_meta_fields >> 32);
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
    printf("[%d] type=%x, seq=%d, node_cnt=%d\n", core_id, op_type, seq, node_cnt);

    if (op_type == CHAINREP_OP_READ) {
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
      printf("[%d] PUT key=%lu val=%lu\n", core_id, msg_key, msg_val);
    }

    // TODO: what dst_context should be used when replying to the client?
    //uint16_t dst_context = (app_hdr & CONTEXT_MASK) >> 16;
    unsigned msg_len = 8 + (node_cnt * 8) + 8 + 8;
    app_hdr = ((uint64_t)dst_ip << 32) | (dst_context << 16) | msg_len;
    lnic_write_r(app_hdr);

    flags = 0;
    cr_meta_fields = ((uint64_t)flags << 56) | ((uint64_t)op_type << 48) | ((uint64_t)seq << 40) | ((uint64_t)node_cnt << 32) | client_ip;
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

bool is_single_core() { return false; }

int core_main(int argc, char** argv, int cid, int nc) {
  (void)nc;
  for (int i = 1; i < argc; i++) {
    printf("argv[%d]: %s\n", i, argv[i]);
  }
  if (cid > 3) return 0;

  uint64_t context_id = cid;
  uint64_t priority = 0;
  lnic_add_context(context_id, priority);

  process_msgs(cid);

  return EXIT_SUCCESS;
}

}
