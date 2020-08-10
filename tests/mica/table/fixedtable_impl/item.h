#pragma once

namespace mica {
namespace table {

template <class StaticConfig>
void FixedTable<StaticConfig>::set_item(Bucket *located_bucket,
                                        size_t item_index, ft_key_t key,
                                        const char *value) {
  located_bucket->key_arr[item_index] = key;
  //uint8_t *_val = get_value(located_bucket, item_index);
  //::mica::util::memcpy(_val, value, val_size);
  //memcpy(_val, value, val_size);
  (void)value;
  uint64_t *_val = (uint64_t *)get_value(located_bucket, item_index);
  _val[0] = lnic_read();
  _val[1] = lnic_read();
  _val[2] = lnic_read();
  _val[3] = lnic_read();
  _val[4] = lnic_read();
  _val[5] = lnic_read();
  _val[6] = lnic_read();
  _val[7] = lnic_read();
#if VALUE_SIZE_WORDS > 8
  _val[8] = lnic_read();
  _val[9] = lnic_read();
  _val[10] = lnic_read();
  _val[11] = lnic_read();
  _val[12] = lnic_read();
  _val[13] = lnic_read();
  _val[14] = lnic_read();
  _val[15] = lnic_read();
  _val[16] = lnic_read();
  _val[17] = lnic_read();
  _val[18] = lnic_read();
  _val[19] = lnic_read();
  _val[20] = lnic_read();
  _val[21] = lnic_read();
  _val[22] = lnic_read();
  _val[23] = lnic_read();
  _val[24] = lnic_read();
  _val[25] = lnic_read();
  _val[26] = lnic_read();
  _val[27] = lnic_read();
  _val[28] = lnic_read();
  _val[29] = lnic_read();
  _val[30] = lnic_read();
  _val[31] = lnic_read();
  _val[32] = lnic_read();
  _val[33] = lnic_read();
  _val[34] = lnic_read();
  _val[35] = lnic_read();
  _val[36] = lnic_read();
  _val[37] = lnic_read();
  _val[38] = lnic_read();
  _val[39] = lnic_read();
  _val[40] = lnic_read();
  _val[41] = lnic_read();
  _val[42] = lnic_read();
  _val[43] = lnic_read();
  _val[44] = lnic_read();
  _val[45] = lnic_read();
  _val[46] = lnic_read();
  _val[47] = lnic_read();
  _val[48] = lnic_read();
  _val[49] = lnic_read();
  _val[50] = lnic_read();
  _val[51] = lnic_read();
  _val[52] = lnic_read();
  _val[53] = lnic_read();
  _val[54] = lnic_read();
  _val[55] = lnic_read();
  _val[56] = lnic_read();
  _val[57] = lnic_read();
  _val[58] = lnic_read();
  _val[59] = lnic_read();
  _val[60] = lnic_read();
  _val[61] = lnic_read();
  _val[62] = lnic_read();
  _val[63] = lnic_read();
#endif // VALUE_SIZE_WORDS > 8
}
}
}
