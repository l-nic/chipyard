#pragma once

namespace mica {
namespace table {

template <class StaticConfig>
void FixedTable<StaticConfig>::set_item(Bucket *located_bucket,
                                        size_t item_index, ft_key_t key,
                                        const char *value) {
  located_bucket->key_arr[item_index] = key;

  uint8_t *_val = get_value(located_bucket, item_index);

  if (value == NULL) {
    uint64_t *_val64 = (uint64_t *)_val;
    _val64[0] = lnic_read();
    _val64[1] = lnic_read();
    _val64[2] = lnic_read();
    _val64[3] = lnic_read();
    _val64[4] = lnic_read();
    _val64[5] = lnic_read();
    _val64[6] = lnic_read();
    _val64[7] = lnic_read();
#if VALUE_SIZE_WORDS > 8
    _val64[8] = lnic_read();
    _val64[9] = lnic_read();
    _val64[10] = lnic_read();
    _val64[11] = lnic_read();
    _val64[12] = lnic_read();
    _val64[13] = lnic_read();
    _val64[14] = lnic_read();
    _val64[15] = lnic_read();
    _val64[16] = lnic_read();
    _val64[17] = lnic_read();
    _val64[18] = lnic_read();
    _val64[19] = lnic_read();
    _val64[20] = lnic_read();
    _val64[21] = lnic_read();
    _val64[22] = lnic_read();
    _val64[23] = lnic_read();
    _val64[24] = lnic_read();
    _val64[25] = lnic_read();
    _val64[26] = lnic_read();
    _val64[27] = lnic_read();
    _val64[28] = lnic_read();
    _val64[29] = lnic_read();
    _val64[30] = lnic_read();
    _val64[31] = lnic_read();
    _val64[32] = lnic_read();
    _val64[33] = lnic_read();
    _val64[34] = lnic_read();
    _val64[35] = lnic_read();
    _val64[36] = lnic_read();
    _val64[37] = lnic_read();
    _val64[38] = lnic_read();
    _val64[39] = lnic_read();
    _val64[40] = lnic_read();
    _val64[41] = lnic_read();
    _val64[42] = lnic_read();
    _val64[43] = lnic_read();
    _val64[44] = lnic_read();
    _val64[45] = lnic_read();
    _val64[46] = lnic_read();
    _val64[47] = lnic_read();
    _val64[48] = lnic_read();
    _val64[49] = lnic_read();
    _val64[50] = lnic_read();
    _val64[51] = lnic_read();
    _val64[52] = lnic_read();
    _val64[53] = lnic_read();
    _val64[54] = lnic_read();
    _val64[55] = lnic_read();
    _val64[56] = lnic_read();
    _val64[57] = lnic_read();
    _val64[58] = lnic_read();
    _val64[59] = lnic_read();
    _val64[60] = lnic_read();
    _val64[61] = lnic_read();
    _val64[62] = lnic_read();
    _val64[63] = lnic_read();
#endif // VALUE_SIZE_WORDS > 8
  }
  else {
    memcpy(_val, value, val_size);
  }
}
}
}
