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

    if (out_value == NULL) {
      uint64_t* _val64 = (uint64_t*)_val;
      lnic_write_r(_val64[0]);
      lnic_write_r(_val64[1]);
      lnic_write_r(_val64[2]);
      lnic_write_r(_val64[3]);
      lnic_write_r(_val64[4]);
      lnic_write_r(_val64[5]);
      lnic_write_r(_val64[6]);
      lnic_write_r(_val64[7]);
#if VALUE_SIZE_WORDS > 8
      lnic_write_r(_val64[8]);
      lnic_write_r(_val64[9]);
      lnic_write_r(_val64[10]);
      lnic_write_r(_val64[11]);
      lnic_write_r(_val64[12]);
      lnic_write_r(_val64[13]);
      lnic_write_r(_val64[14]);
      lnic_write_r(_val64[15]);
      lnic_write_r(_val64[16]);
      lnic_write_r(_val64[17]);
      lnic_write_r(_val64[18]);
      lnic_write_r(_val64[19]);
      lnic_write_r(_val64[20]);
      lnic_write_r(_val64[21]);
      lnic_write_r(_val64[22]);
      lnic_write_r(_val64[23]);
      lnic_write_r(_val64[24]);
      lnic_write_r(_val64[25]);
      lnic_write_r(_val64[26]);
      lnic_write_r(_val64[27]);
      lnic_write_r(_val64[28]);
      lnic_write_r(_val64[29]);
      lnic_write_r(_val64[30]);
      lnic_write_r(_val64[31]);
      lnic_write_r(_val64[32]);
      lnic_write_r(_val64[33]);
      lnic_write_r(_val64[34]);
      lnic_write_r(_val64[35]);
      lnic_write_r(_val64[36]);
      lnic_write_r(_val64[37]);
      lnic_write_r(_val64[38]);
      lnic_write_r(_val64[39]);
      lnic_write_r(_val64[40]);
      lnic_write_r(_val64[41]);
      lnic_write_r(_val64[42]);
      lnic_write_r(_val64[43]);
      lnic_write_r(_val64[44]);
      lnic_write_r(_val64[45]);
      lnic_write_r(_val64[46]);
      lnic_write_r(_val64[47]);
      lnic_write_r(_val64[48]);
      lnic_write_r(_val64[49]);
      lnic_write_r(_val64[50]);
      lnic_write_r(_val64[51]);
      lnic_write_r(_val64[52]);
      lnic_write_r(_val64[53]);
      lnic_write_r(_val64[54]);
      lnic_write_r(_val64[55]);
      lnic_write_r(_val64[56]);
      lnic_write_r(_val64[57]);
      lnic_write_r(_val64[58]);
      lnic_write_r(_val64[59]);
      lnic_write_r(_val64[60]);
      lnic_write_r(_val64[61]);
      lnic_write_r(_val64[62]);
      lnic_write_r(_val64[63]);
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
