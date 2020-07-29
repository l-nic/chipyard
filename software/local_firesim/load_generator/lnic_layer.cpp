#include "lnic_layer.h"
#include <string.h>
#include <sstream>

namespace pcpp
{

LnicLayer::LnicLayer(uint8_t flags, uint16_t src_context, uint16_t dst_context,
                     uint16_t msg_len, uint8_t pkt_offset,
                     uint16_t pull_offset, uint16_t tx_msg_id,
                     uint16_t buf_ptr, uint8_t buf_size_class) {
    const size_t header_len = sizeof(lnichdr);
    m_DataLen = header_len;
    m_Data = new uint8_t[header_len];
    memset(m_Data, 0, header_len);
    lnichdr* lnic_hdr = (lnichdr*)m_Data;
    m_Protocol = GenericPayload;
    lnic_hdr->flags = flags;
    lnic_hdr->src_context = htobe16(src_context);
    lnic_hdr->dst_context = htobe16(dst_context);
    lnic_hdr->msg_len = htobe16(msg_len);
    lnic_hdr->pkt_offset = pkt_offset;
    lnic_hdr->pull_offset = htobe16(pull_offset);
    lnic_hdr->tx_msg_id = htobe16(tx_msg_id);
    lnic_hdr->buf_ptr = htobe16(buf_ptr);
    lnic_hdr->buf_size_class = buf_size_class;
}

std::string LnicLayer::toString() const {
    std::ostringstream output_string;
    fprintf(stdout, "getting string\n");
    lnichdr* lnic_hdr = getLnicHeader();
    std::string flags_str;
    uint8_t lnic_header_flags = lnic_hdr->flags;
    bool is_data = lnic_header_flags & LNIC_DATA_FLAG_MASK;
    bool is_ack = lnic_header_flags & LNIC_ACK_FLAG_MASK;
    bool is_nack = lnic_header_flags & LNIC_NACK_FLAG_MASK;
    bool is_pull = lnic_header_flags & LNIC_PULL_FLAG_MASK;
    bool is_chop = lnic_header_flags & LNIC_CHOP_FLAG_MASK;
    flags_str += is_data ? "DATA" : "";
    flags_str += is_ack ? " ACK" : "";
    flags_str += is_nack ? " NACK" : "";
    flags_str += is_pull ? " PULL" : "";
    flags_str += is_chop ? " CHOP" : "";
    output_string << "LNIC(flags=" << flags_str << ", src_context=" << be16toh(lnic_hdr->src_context)
                  << ", dst_context=" << be16toh(lnic_hdr->dst_context) << ", msg_len="
                  << be16toh(lnic_hdr->msg_len) << ", pkt_offset=" << lnic_hdr->pkt_offset
                  << ", pull_offset=" << be16toh(lnic_hdr->pull_offset) << ", tx_msg_id="
                  << be16toh(lnic_hdr->pkt_offset) << ", buf_ptr=" << be16toh(lnic_hdr->buf_ptr)
                  << ", buf_size_class=" << lnic_hdr->buf_size_class << ")";
    return output_string.str();
}

}