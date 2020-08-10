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

#if USE_MICA_LNIC
    (void)out_value;
    uint64_t* _val = (uint64_t*)get_value(located_bucket, item_index);
    lnic_write_r(_val[0]);
    lnic_write_r(_val[1]);
    lnic_write_r(_val[2]);
    lnic_write_r(_val[3]);
    lnic_write_r(_val[4]);
    lnic_write_r(_val[5]);
    lnic_write_r(_val[6]);
    lnic_write_r(_val[7]);
#if VALUE_SIZE_WORDS > 8
    lnic_write_r(_val[8]);
    lnic_write_r(_val[9]);
    lnic_write_r(_val[10]);
    lnic_write_r(_val[11]);
    lnic_write_r(_val[12]);
    lnic_write_r(_val[13]);
    lnic_write_r(_val[14]);
    lnic_write_r(_val[15]);
    lnic_write_r(_val[16]);
    lnic_write_r(_val[17]);
    lnic_write_r(_val[18]);
    lnic_write_r(_val[19]);
    lnic_write_r(_val[20]);
    lnic_write_r(_val[21]);
    lnic_write_r(_val[22]);
    lnic_write_r(_val[23]);
    lnic_write_r(_val[24]);
    lnic_write_r(_val[25]);
    lnic_write_r(_val[26]);
    lnic_write_r(_val[27]);
    lnic_write_r(_val[28]);
    lnic_write_r(_val[29]);
    lnic_write_r(_val[30]);
    lnic_write_r(_val[31]);
    lnic_write_r(_val[32]);
    lnic_write_r(_val[33]);
    lnic_write_r(_val[34]);
    lnic_write_r(_val[35]);
    lnic_write_r(_val[36]);
    lnic_write_r(_val[37]);
    lnic_write_r(_val[38]);
    lnic_write_r(_val[39]);
    lnic_write_r(_val[40]);
    lnic_write_r(_val[41]);
    lnic_write_r(_val[42]);
    lnic_write_r(_val[43]);
    lnic_write_r(_val[44]);
    lnic_write_r(_val[45]);
    lnic_write_r(_val[46]);
    lnic_write_r(_val[47]);
    lnic_write_r(_val[48]);
    lnic_write_r(_val[49]);
    lnic_write_r(_val[50]);
    lnic_write_r(_val[51]);
    lnic_write_r(_val[52]);
    lnic_write_r(_val[53]);
    lnic_write_r(_val[54]);
    lnic_write_r(_val[55]);
    lnic_write_r(_val[56]);
    lnic_write_r(_val[57]);
    lnic_write_r(_val[58]);
    lnic_write_r(_val[59]);
    lnic_write_r(_val[60]);
    lnic_write_r(_val[61]);
    lnic_write_r(_val[62]);
    lnic_write_r(_val[63]);
#endif // VALUE_SIZE_WORDS > 8

#else
    uint8_t* _val = get_value(located_bucket, item_index);
    //::mica::util::memcpy<8>(out_value, _val, val_size);
    memcpy(out_value, _val, val_size);
#endif // USE_MICA_LNIC

    if (version_start != read_version_end(bucket)) continue; /* Try again */

    stat_inc(&Stats::get_found);
    break;
  }

  return Result::kSuccess;
}
}
}
