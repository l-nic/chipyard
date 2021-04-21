// ----------------------------------------------------------------------- //
// homa.p4
// ----------------------------------------------------------------------- //

const bit<8> HOMA_PROTO = 154;

const bit<8> DATA_MASK  = 0b00000001;
const bit<8> ACK_MASK   = 0b00000010;
const bit<8> NACK_MASK  = 0b00000100;
const bit<8> GRANT_MASK = 0b00001000;
const bit<8> CHOP_MASK  = 0b00010000;

header homa_t {
    bit<8> flags;
    ContextID_t src;
    ContextID_t dst;
    bit<16> msg_len;
    bit<8> pkt_offset;
    bit<16> grant_offset;
    bit<8> grant_prio;
    MsgID_t tx_msg_id;
    bit<16> buf_ptr;
    bit<8> buf_size_class;
    // padding to make header len = 64B for easy parsing / deparsing (for Chisel implementation)
    bit<112> padding;
}

// header structure
struct headers {
    eth_mac_t  eth;
    ipv4_t     ipv4;
    homa_t     homa;
}

