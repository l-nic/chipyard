
#define CONFIG_TYPE 0
#define DATA_TYPE 1
#define RESP_TYPE 2

#define NUM_WEIGHTS 2000

struct dot_product_header {
  uint64_t type;
};

struct config_header {
  uint64_t num_msgs;
  uint64_t timestamp;
};

struct data_header {
  uint64_t num_words;
};

struct resp_header {
  uint64_t result;
  uint64_t cache_misses;
  uint64_t timestamp;
};

