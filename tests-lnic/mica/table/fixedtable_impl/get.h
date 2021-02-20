#pragma once

namespace mica {
namespace table {
template <class StaticConfig>
/**
 * @param key_hash The hash of the key computed using mica::util::hash
 * @param key The key to get()
 * @param out_value Pointer to a buffer to copy the value to. The buffer should
 * have space for StaticConfig::kValSize bytes
 */
Result FixedTable<StaticConfig>::get(uint64_t key_hash, const ft_key_t& key,
                                     char* out_value) const {
  uint32_t bucket_index = calc_bucket_index(key_hash);
  const Bucket* bucket = get_bucket(bucket_index);

  while (true) {
    uint32_t version_start = read_version_begin(bucket);

    const Bucket* located_bucket;
    size_t item_index = find_item_index(bucket, key, &located_bucket);
    if (item_index == StaticConfig::kBucketCap) {
      if (version_start != read_version_end(bucket)) continue; /* Try again */
      stat_inc(&Stats::get_notfound);
      return Result::kNotFound;
    }

    uint8_t* _val = get_value(located_bucket, item_index);

//#define MICA_LNIC_WRITE_FUNC lnic_write_r
#define MICA_LNIC_WRITE_FUNC lnic_write_m

    if (out_value == NULL) {
      uint64_t* _val64 = (uint64_t*)_val;
      MICA_LNIC_WRITE_FUNC(_val64[0]);
      MICA_LNIC_WRITE_FUNC(_val64[1]);
      MICA_LNIC_WRITE_FUNC(_val64[2]);
      MICA_LNIC_WRITE_FUNC(_val64[3]);
      MICA_LNIC_WRITE_FUNC(_val64[4]);
      MICA_LNIC_WRITE_FUNC(_val64[5]);
      MICA_LNIC_WRITE_FUNC(_val64[6]);
      MICA_LNIC_WRITE_FUNC(_val64[7]);
#if VALUE_SIZE_WORDS > 8
      MICA_LNIC_WRITE_FUNC(_val64[8]);
      MICA_LNIC_WRITE_FUNC(_val64[9]);
      MICA_LNIC_WRITE_FUNC(_val64[10]);
      MICA_LNIC_WRITE_FUNC(_val64[11]);
      MICA_LNIC_WRITE_FUNC(_val64[12]);
      MICA_LNIC_WRITE_FUNC(_val64[13]);
      MICA_LNIC_WRITE_FUNC(_val64[14]);
      MICA_LNIC_WRITE_FUNC(_val64[15]);
      MICA_LNIC_WRITE_FUNC(_val64[16]);
      MICA_LNIC_WRITE_FUNC(_val64[17]);
      MICA_LNIC_WRITE_FUNC(_val64[18]);
      MICA_LNIC_WRITE_FUNC(_val64[19]);
      MICA_LNIC_WRITE_FUNC(_val64[20]);
      MICA_LNIC_WRITE_FUNC(_val64[21]);
      MICA_LNIC_WRITE_FUNC(_val64[22]);
      MICA_LNIC_WRITE_FUNC(_val64[23]);
      MICA_LNIC_WRITE_FUNC(_val64[24]);
      MICA_LNIC_WRITE_FUNC(_val64[25]);
      MICA_LNIC_WRITE_FUNC(_val64[26]);
      MICA_LNIC_WRITE_FUNC(_val64[27]);
      MICA_LNIC_WRITE_FUNC(_val64[28]);
      MICA_LNIC_WRITE_FUNC(_val64[29]);
      MICA_LNIC_WRITE_FUNC(_val64[30]);
      MICA_LNIC_WRITE_FUNC(_val64[31]);
      MICA_LNIC_WRITE_FUNC(_val64[32]);
      MICA_LNIC_WRITE_FUNC(_val64[33]);
      MICA_LNIC_WRITE_FUNC(_val64[34]);
      MICA_LNIC_WRITE_FUNC(_val64[35]);
      MICA_LNIC_WRITE_FUNC(_val64[36]);
      MICA_LNIC_WRITE_FUNC(_val64[37]);
      MICA_LNIC_WRITE_FUNC(_val64[38]);
      MICA_LNIC_WRITE_FUNC(_val64[39]);
      MICA_LNIC_WRITE_FUNC(_val64[40]);
      MICA_LNIC_WRITE_FUNC(_val64[41]);
      MICA_LNIC_WRITE_FUNC(_val64[42]);
      MICA_LNIC_WRITE_FUNC(_val64[43]);
      MICA_LNIC_WRITE_FUNC(_val64[44]);
      MICA_LNIC_WRITE_FUNC(_val64[45]);
      MICA_LNIC_WRITE_FUNC(_val64[46]);
      MICA_LNIC_WRITE_FUNC(_val64[47]);
      MICA_LNIC_WRITE_FUNC(_val64[48]);
      MICA_LNIC_WRITE_FUNC(_val64[49]);
      MICA_LNIC_WRITE_FUNC(_val64[50]);
      MICA_LNIC_WRITE_FUNC(_val64[51]);
      MICA_LNIC_WRITE_FUNC(_val64[52]);
      MICA_LNIC_WRITE_FUNC(_val64[53]);
      MICA_LNIC_WRITE_FUNC(_val64[54]);
      MICA_LNIC_WRITE_FUNC(_val64[55]);
      MICA_LNIC_WRITE_FUNC(_val64[56]);
      MICA_LNIC_WRITE_FUNC(_val64[57]);
      MICA_LNIC_WRITE_FUNC(_val64[58]);
      MICA_LNIC_WRITE_FUNC(_val64[59]);
      MICA_LNIC_WRITE_FUNC(_val64[60]);
      MICA_LNIC_WRITE_FUNC(_val64[61]);
      MICA_LNIC_WRITE_FUNC(_val64[62]);
      MICA_LNIC_WRITE_FUNC(_val64[63]);
#endif // VALUE_SIZE_WORDS > 8
    }
    else {
      memcpy(out_value, _val, val_size);
    }

    if (version_start != read_version_end(bucket)) continue; /* Try again */

    stat_inc(&Stats::get_found);
    break;
  }

  return Result::kSuccess;
}
}
}
