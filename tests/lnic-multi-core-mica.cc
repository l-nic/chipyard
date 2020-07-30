#include <stdio.h>
#include <stdlib.h>

#include "mica/table/fixedtable.h"
#include "mica/util/hash.h"
#include "lnic.h"

#define MICA_R_TYPE 1
#define MICA_W_TYPE 2

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

int process_msgs(int core_id) {
	uint64_t app_hdr;
  uint64_t msg_type;
  uint64_t msg_key;
  uint64_t key_hash;
  uint64_t msg_val;
	uint16_t msg_len;
  MicaResult out_result;
  FixedTable::ft_key_t ft_key;
  FixedTable table(kValSize, core_id);

  printf("[%d] ready.\n", core_id);

	while (1) {
		lnic_wait();
		app_hdr = lnic_read();
		msg_len = (uint16_t)app_hdr;
		printf("[%d] --> Received msg of length: %u bytes\n", core_id, msg_len);
    msg_type = lnic_read();
    msg_key = lnic_read();

    printf("[%d] type=%lu, key=%lu\n", core_id, msg_type, msg_key);
    key_hash = mica_hash(&msg_key, sizeof(msg_key));
    ft_key.qword[0] = msg_key;

    if (msg_type == MICA_R_TYPE) {
      out_result = table.get(key_hash, ft_key, reinterpret_cast<char *>(&msg_val));
      if (out_result != MicaResult::kSuccess) {
        printf("[%d] GET failed for key %lu.\n", core_id, msg_key);
      }
      printf("[%d] GET key=%lu val=%lu\n", core_id, msg_key, msg_val);
    }
    else {
      msg_val = lnic_read();
      printf("[%d] PUT key=%lu val=%lu\n", core_id, msg_key, msg_val);
      out_result = table.set(key_hash, ft_key, reinterpret_cast<char *>(&msg_val));
      if (out_result != MicaResult::kSuccess) {
        printf("[%d] Inserting key %lu failed.\n", core_id, msg_key);
      }
    }

    lnic_write_r((app_hdr & (IP_MASK | CONTEXT_MASK)) | 8);
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
