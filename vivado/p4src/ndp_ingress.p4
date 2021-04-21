// ----------------------------------------------------------------------- //
// ndp_ingress.p4
// ----------------------------------------------------------------------- //

#include <core.p4>
#include <xsa.p4>
#include "lnic.p4"
#include "ndp.p4"

/* IO for ifElseRaw extern */
const bit<8> REG_READ  = 0;
const bit<8> REG_WRITE = 1;
const bit<8> REG_ADD   = 2;
struct ifElseRaw_req_t {
  MsgID_t index;
  bit<16> data_1;
  bit<8>  opCode_1;
  bit<16> data_0;
  bit<8>  opCode_0;
  bool    predicate;
}

struct ifElseRaw_resp_t {
  bit<16> new_val;
}

// ****************************************************************************** //
// *************************** P A R S E R  ************************************* //
// ****************************************************************************** //

parser MyParser(packet_in packet, 
                out headers hdr, 
                inout full_ingress_metadata_t meta, 
                inout standard_metadata_t smeta) {

    state start {
        transition parse_eth;
    }

    state parse_eth {
        packet.extract(hdr.eth);
        transition select(hdr.eth.etherType) {
            IPV4_TYPE : parse_ipv4;
            default   : accept; 
        }
    }

    state parse_ipv4 {
        packet.extract(hdr.ipv4);
        transition select(hdr.ipv4.protocol) {
            NDP_PROTO : parse_ndp;
            default    : accept; 
        }
    }
    
    state parse_ndp {
        packet.extract(hdr.ndp);
        transition accept;
    }

}

// ****************************************************************************** //
// **************************  P R O C E S S I N G   **************************** //
// ****************************************************************************** //

control MyProcessing(inout headers hdr, 
                     inout full_ingress_metadata_t meta, 
                     inout standard_metadata_t smeta) {

    UserExtern<get_rx_msg_info_req_t, get_rx_msg_info_resp_t>(2) get_rx_msg_info;
    UserExtern<delivered_meta_t, dummy_t>(1) delivered_event;
    UserExtern<creditToBtx_meta_t, dummy_t>(1) creditToBtx_event;
    UserExtern<egress_metadata_t, dummy_t>(1) ctrlPkt_event;

    // state used to compute pull offset
    UserExtern<ifElseRaw_req_t, ifElseRaw_resp_t>(2) credit_ifElseRaw;

    apply {
        // Process all pkts arriving at the NIC
        if (hdr.ndp.isValid()) {
            dummy_t dummy; // unused metadata for events
            if ((hdr.ndp.flags & DATA_MASK) > 0) {
                // This is a DATA pkt, but it might be CHOP'ed
                bool is_chopped = (hdr.ndp.flags & CHOP_MASK) > 0;

                // get info about msg from message assembly module
                get_rx_msg_info_req_t req;
                req.mark_received = !is_chopped;
                req.src_ip        = hdr.ipv4.srcAddr;
                req.src_context   = hdr.ndp.src_context;
                req.tx_msg_id     = hdr.ndp.tx_msg_id;
                req.msg_len       = hdr.ndp.msg_len;
                req.pkt_offset    = hdr.ndp.pkt_offset;
                get_rx_msg_info_resp_t rx_msg_info;
                get_rx_msg_info.apply(req, rx_msg_info);

                bool genACK  = false; // default
                bool genNACK = false; // default
                bool genPULL = true;
                bit<16> pull_offset_diff = 0;
                if (is_chopped) {
                    // DATA pkt has been chopped ==> send NACK
                    genNACK = true;
                    smeta.drop = 1;
                } else {
                    // DATA pkt is intact ==> send ACK
                    genACK = true;
                    pull_offset_diff = 1; // increase pull offset

                    // Fill out metadata for packets going to CPU
                    meta.meta.src_ip      = hdr.ipv4.srcAddr;
                    meta.meta.src_context = hdr.ndp.src;
                    meta.meta.msg_len     = hdr.ndp.msg_len;
                    meta.meta.pkt_offset  = hdr.ndp.pkt_offset;
                    meta.meta.dst_context = hdr.ndp.dst;
                    meta.meta.rx_msg_id   = rx_msg_info.rx_msg_id;
                    meta.meta.tx_msg_id   = hdr.ndp.tx_msg_id;
                    meta.meta.is_last_pkt = rx_msg_info.is_last_pkt;
                }

                if (rx_msg_info.fail) {
                    // failed to allocate rx buffer ==> drop pkt
                    smeta.drop = 1;
                } else {
                    // compute PULL offset
                    ifElseRaw_req_t credit_req;
                    credit_req.index      = rx_msg_info.rx_msg_id;
                    credit_req.data_1     = pull_offset_diff;
                    credit_req.opCode_1   = REG_ADD;
                    credit_req.data_0     = meta.params.rtt_pkts + pull_offset_diff;
                    credit_req.opCode_0   = REG_WRITE;
                    credit_req.predicate  = rx_msg_info.is_new_msg;
                    ifElseRaw_resp_t credit_resp;
                    credit_ifElseRaw.apply(credit_req, credit_resp);

                    bit<16> pull_offset = credit_resp.new_val;

                    bit<8> ack_flag  = genACK  ? ACK_MASK  : 0;
                    bit<8> nack_flag = genNACK ? NACK_MASK : 0;
                    bit<8> pull_flag = genPULL ? PULL_MASK : 0;
                    bit<8> flags = ack_flag | nack_flag | pull_flag;

                    // generate ctrl pkt(s)
                    egress_metadata_t ctrlPkt_meta;
                    ctrlPkt_meta.dst_ip         = hdr.ipv4.srcAddr;
                    ctrlPkt_meta.dst_context    = hdr.ndp.src_context;
                    ctrlPkt_meta.msg_len        = hdr.ndp.msg_len;
                    ctrlPkt_meta.pkt_offset     = hdr.ndp.pkt_offset;
                    ctrlPkt_meta.src_context    = hdr.ndp.dst_context;
                    ctrlPkt_meta.tx_msg_id      = hdr.ndp.tx_msg_id;
                    ctrlPkt_meta.buf_ptr        = hdr.ndp.buf_ptr;
                    ctrlPkt_meta.buf_size_class = hdr.ndp.buf_size_class;
                    ctrlPkt_meta.grant_offset   = pull_offset;
                    ctrlPkt_meta.grant_prio     = 0;
                    ctrlPkt_meta.flags          = flags;
                    ctrlPkt_meta.is_new_msg     = false;
                    ctrlPkt_meta.is_rtx         = false;

                    ctrlPkt_event.apply(ctrlPkt_meta, dummy);
                }

                hdr.eth.setInvalid();
                hdr.ipv4.setInvalid();
                hdr.ndp.setInvalid();
            } else {
                // Processing control pkt (ACK / NACK / PULL)
                // do not pass ctrl pkts to assembly module
                smeta.drop = 1;
                if ((hdr.ndp.flags & ACK_MASK) > 0) {
                    // fire delivered event
                    delivered_meta_t delivered_meta;
                    delivered_meta.tx_msg_id      = hdr.ndp.tx_msg_id;
                    delivered_meta.delivered_pkts = (1 << hdr.ndp.pkt_offset);
                    delivered_meta.msg_len        = hdr.ndp.msg_len;
                    delivered_meta.buf_ptr        = hdr.ndp.buf_ptr;
                    delivered_meta.buf_size_class = hdr.ndp.buf_size_class;

                    delivered_event.apply(delivered_meta, dummy);
                }
                if ((hdr.ndp.flags & NACK_MASK) > 0 || (hdr.ndp.flags & PULL_MASK) > 0) {
                    bool rtx = (hdr.ndp.flags & NACK_MASK > 0);
                    bool update_credit = (hdr.ndp.flags & PULL_MASK) > 0;

                    // fire creditToBtx event
                    creditToBtx_meta_t creditToBtx_meta;
                    creditToBtx_meta.tx_msg_id       = hdr.ndp.tx_msg_id;
                    creditToBtx_meta.rtx             = rtx;
                    creditToBtx_meta.rtx_pkt_offset  = hdr.ndp.pkt_offset;
                    creditToBtx_meta.update_credit   = update_credit;
                    creditToBtx_meta.new_credit      = hdr.ndp.pull_offset;
                    creditToBtx_meta.buf_ptr         = hdr.ndp.buf_ptr;
                    creditToBtx_meta.buf_size_class  = hdr.ndp.buf_size_class;
                    creditToBtx_meta.dst_ip          = hdr.ipv4.srcAddr;
                    creditToBtx_meta.dst_context     = hdr.ndp.src;
                    creditToBtx_meta.msg_len         = hdr.ndp.msg_len;
                    creditToBtx_meta.src_context     = hdr.ndp.dst;

                    creditToBtx_event.apply(creditToBtx_meta, dummy);
                }
            }
        } else {
            // non-NDP packet from network
            smeta.drop = 1;
        }
    }
} 

// ****************************************************************************** //
// ***************************  D E P A R S E R  ******************************** //
// ****************************************************************************** //

control MyDeparser(packet_out packet, 
                   in headers hdr,
                   inout full_ingress_metadata_t meta, 
                   inout standard_metadata_t smeta) {
    apply { }
}

// ****************************************************************************** //
// *******************************  M A I N  ************************************ //
// ****************************************************************************** //

XilinxPipeline(
    MyParser(), 
    MyProcessing(), 
    MyDeparser()
) main;
