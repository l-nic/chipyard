#pragma once

#define NCORES 4
#define MICA_SHM_BUFFER_SIZE (3 * 1024 * 1024)
char nanopu_mica_buffer[NCORES][MICA_SHM_BUFFER_SIZE];

namespace mica {
namespace table {
template <class StaticConfig>
FixedTable<StaticConfig>::FixedTable(size_t val_size, int core_id)
    : val_size(val_size),
    core_id(core_id),
      ft_invalid_key(invalid_key()) {
  //assert(val_size % sizeof(uint64_t) == 0);  // Make buckets 8-byte aligned

  // The Bucket struct does not contain values
  bkt_size_with_val = sizeof(Bucket) + (val_size * StaticConfig::kBucketCap);

  //name = config.get("name").get_str();
  //size_t item_count = config.get("item_count").get_uint64();
  //name = "test_table";
  size_t item_count = StaticConfig::itemCount;
  // Compensate the load factor.
  item_count = item_count * 11 / 10;

  size_t num_buckets =
      (item_count + StaticConfig::kBucketCap - 1) / StaticConfig::kBucketCap;
  //assert(num_buckets > 0, "");

  double extra_collision_avoidance = 0.1;
  size_t num_extra_buckets = static_cast<size_t>(
      static_cast<double>(num_buckets) * extra_collision_avoidance);

  //bool concurrent_read = config.get("concurrent_read").get_bool();
  //bool concurrent_write = config.get("concurrent_write").get_bool();
  bool concurrent_read = StaticConfig::concurrentRead;
  bool concurrent_write = StaticConfig::concurrentWrite;

  size_t log_num_buckets = 0;
  while ((1ull << log_num_buckets) < num_buckets) log_num_buckets++;
  //assert(log_num_buckets <= 32, "");

  num_buckets = 1ull << log_num_buckets;
  //assert(num_buckets < std::numeric_limits<uint32_t>::max() / 2, "");
  //assert(num_extra_buckets < std::numeric_limits<uint32_t>::max() / 2, "");

  num_buckets_ = static_cast<uint32_t>(num_buckets);
  num_buckets_mask_ = static_cast<uint32_t>(num_buckets - 1);
  num_extra_buckets_ = static_cast<uint32_t>(num_extra_buckets);

  {
    size_t shm_size = bkt_size_with_val * (num_buckets_ + num_extra_buckets_);
    printf("shm_size: %lu\n", shm_size);

    // TODO: Extend num_extra_buckets_ to meet shm_size.

    // Zeroes out everything
    //buckets_ = reinterpret_cast<Bucket*>(malloc(shm_size));
    buckets_ = reinterpret_cast<Bucket*>(nanopu_mica_buffer[core_id]);
    //assert(buckets_ != NULL);
    memset(buckets_, 0, shm_size);  // alloc_raw does not give zeroed memory
  }

  // subtract by one to compensate 1-base indices
  extra_buckets_ =
      reinterpret_cast<Bucket*>(reinterpret_cast<uint8_t*>(buckets_) +
                                ((num_buckets_ - 1) * bkt_size_with_val));
  // the rest extra_bucket information is initialized in reset()

  if (!concurrent_read)
    concurrent_access_mode_ = 0;
  else if (concurrent_read && !concurrent_write)
    concurrent_access_mode_ = 1;
  else
    concurrent_access_mode_ = 2;

  reset();

  if (StaticConfig::kVerbose) {
    fprintf(stderr, "warning: kVerbose is defined (low performance)\n");

    if (StaticConfig::kCollectStats)
      fprintf(stderr, "warning: kCollectStats is defined (low performance)\n");

    if (StaticConfig::kConcurrent && !concurrent_read && !concurrent_write)
      fprintf(stderr, "warning: kConcurrent is defined (low performance)\n");

    fprintf(stderr, "info: num_buckets = %u\n", num_buckets_);
    fprintf(stderr, "info: num_extra_buckets = %u\n", num_extra_buckets_);

    fprintf(stderr, "\n");
  }
}

template <class StaticConfig>
FixedTable<StaticConfig>::~FixedTable() {
  //printf("Destroying table %s\n", name.c_str());
  reset();

  // Table's owner will destroy the hugepage allocator, which will release
  // used hugepages.
}

template <class StaticConfig>
void FixedTable<StaticConfig>::reset() {
  // Initialize bucket metadata + fill in invalid keys
  for (size_t bkt_i = 0; bkt_i < num_buckets_ + num_extra_buckets_; bkt_i++) {
    Bucket* bucket = get_bucket(bkt_i);
    for (size_t item_index = 0; item_index < StaticConfig::kBucketCap;
         item_index++) {
      bucket->key_arr[item_index] = ft_invalid_key;
    }
  }

  // initialize a free list of extra buckets
  extra_bucket_free_list_.lock = 0;

  if (num_extra_buckets_ == 0)
    extra_bucket_free_list_.head = 0;  // no extra bucket at all
  else {
    uint32_t extra_bucket_index;
    for (extra_bucket_index = 1;
         extra_bucket_index < 1 + num_extra_buckets_ - 1;
         extra_bucket_index++) {
      get_extra_bucket(extra_bucket_index)->next_extra_bucket_index =
          extra_bucket_index + 1;
    }

    get_extra_bucket(extra_bucket_index)->next_extra_bucket_index =
        0;  // no more free extra bucket

    extra_bucket_free_list_.head = 1;
  }

  reset_stats(true);
}

template <class StaticConfig>
typename FixedTable<StaticConfig>::ft_key_t
FixedTable<StaticConfig>::invalid_key() const {
  ft_key_t ret;
  for (size_t i = 0; i < StaticConfig::kKeySize / 8; i++) {
    ret.qword[i] = 0xffffffffffffffffull;
  }

  return ret;
}
}
}
