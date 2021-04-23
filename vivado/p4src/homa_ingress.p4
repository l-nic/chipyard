// ----------------------------------------------------------------------- //
// homa_ingress.p4
// ----------------------------------------------------------------------- //

#include <core.p4>
#include <xsa.p4>
#include "lnic.p4"
#include "homa.p4"

// ****************************************************************************** //
// *************************** EXTERN INTERFACES ******************************** //
// ****************************************************************************** //

struct pendingMsgInfo_t {
    IPv4Addr_t src_ip;
    ContextID_t src_context;
    ContextID_t dst_context;
    MsgID_t tx_msg_id;
    bit<16> msg_len;
    bit<16> buf_ptr;
    bit<8> buf_size_class;
    bit<9> ackNo; // ackNo needs to reach max_num_pkts + 1
}

struct grantInfo_t {
    bit<16> grantedIdx;
    bit<16> grantableIdx;
    bit<8> remaining_size;
}

struct curMsgReg_req_t {
    MsgID_t index;
    pendingMsgInfo_t msg_info;
    grantInfo_t grant_info;
    // is_new_msg is used to determine how to update msg state
    bool is_new_msg;
}

struct grantMsgReg_req_t {
    MsgID_t index;
    bit<16> grantedIdx;
}

struct grantSched_req_t {
    MsgID_t rx_msg_id;
    bit<8> rank;
    bool removeObj;
    bit<16> grantableIdx;
    bit<16> grantedIdx;
}

struct grantSched_resp_t {
    bool success; // a msg to grant was identified
    bit<8> prio_level;
    bit<16> grant_offset;
    MsgID_t grant_msg_id;
}

struct txMsgPrioReg_req_t {
    MsgID_t index;
    bit<8> prio;
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
            HOMA_PROTO : parse_homa;
            default    : accept; 
        }
    }
    
    state parse_homa {
        packet.extract(hdr.homa);
        transition accept;
    }

}

// ****************************************************************************** //
// **************************  P R O C E S S I N G   **************************** //
// ****************************************************************************** //

control MyProcessing(inout headers hdr, 
                     inout full_ingress_metadata_t meta, 
                     inout standard_metadata_t smeta) {
    // Externs
    UserExtern<get_rx_msg_info_req_t, get_rx_msg_info_resp_t>(2) get_rx_msg_info;
    UserExtern<delivered_meta_t, dummy_t>(1) delivered_event;
    UserExtern<creditToBtx_meta_t, dummy_t>(1) creditToBtx_event;

    // TODO(sarslan): The 3 externs below can all be one called ctrlPkt_event
    //                because it is guaranteed that only 1 of them are fired per 
    //                arriving packet
    UserExtern<egress_metadata_t, dummy_t>(1) ackPkt_event;
    UserExtern<egress_metadata_t, dummy_t>(1) nackPkt_event;
    UserExtern<egress_metadata_t, dummy_t>(1) grantPkt_event;

    UserExtern<curMsgReg_req_t, grantInfo_t>(2) curMsgReg;
    UserExtern<grantMsgReg_req_t, pendingMsgInfo_t>(2) grantMsgReg;
    UserExtern<grantSched_req_t, grantSched_resp_t>(2) grantScheduler;
    UserExtern<txMsgPrioReg_req_t, dummy_t>(1) txMsgPrioReg;

    apply {
        // Process all pkts arriving at the NIC
        if (hdr.homa.isValid()) {
            dummy_t dummy; // unused metadata for events
            if ((hdr.ndp.flags & DATA_MASK) > 0) {
                // This is a DATA pkt, but it might be CHOP'ed
                bool is_chopped = (hdr.homa.flags & CHOP_MASK) > 0;

                // get info about msg from message assembly module
                get_rx_msg_info_req_t req;
                req.mark_received = !is_chopped;
                req.src_ip        = hdr.ipv4.srcAddr;
                req.src_context   = hdr.homa.src;
                req.tx_msg_id     = hdr.homa.tx_msg_id;
                req.msg_len       = hdr.homa.msg_len;
                req.pkt_offset    = hdr.homa.pkt_offset;
                get_rx_msg_info_resp_t rx_msg_info;

                get_rx_msg_info.apply(req, rx_msg_info);

                egress_metadata_t ctrlPkt_meta;
                ctrlPkt_meta.dst_ip         = hdr.ipv4.srcAddr;
                ctrlPkt_meta.dst_context    = hdr.homa.src;
                ctrlPkt_meta.msg_len        = hdr.homa.msg_len;
                ctrlPkt_meta.src_context    = hdr.homa.dst;
                ctrlPkt_meta.tx_msg_id      = hdr.homa.tx_msg_id;
                ctrlPkt_meta.buf_ptr        = hdr.homa.buf_ptr;
                ctrlPkt_meta.buf_size_class = hdr.homa.buf_size_class;
                ctrlPkt_meta.is_new_msg     = false;
                ctrlPkt_meta.is_rtx         = false;

                if (rx_msg_info.fail || is_chopped) {
                    // generate NACK (Only in Homa-Tr)
                    smeta.drop = 1;

                    ctrlPkt_meta.pkt_offset = hdr.ndp.pkt_offset;
                    ctrlPkt_meta.credit = rx_msg_info.fail ? 0 : rx_msg_info.ackNo;
                    ctrlPkt_meta.flags = NACK_MASK;

                    nackPkt_event.apply(ctrlPkt_meta, dummy);

                } else if (!rx_msg_info.is_new_pkt ) { 
                    smeta.drop = 1;
                } else {
                    // Fill out metadata for packets going to CPU
                    meta.meta.src_ip      = hdr.ipv4.srcAddr;
                    meta.meta.src_context = hdr.homa.src;
                    meta.meta.msg_len     = hdr.homa.msg_len;
                    meta.meta.pkt_offset  = hdr.homa.pkt_offset;
                    meta.meta.dst_context = hdr.homa.dst;
                    meta.meta.rx_msg_id   = rx_msg_info.rx_msg_id;
                    meta.meta.tx_msg_id   = hdr.homa.tx_msg_id;
                    meta.meta.is_last_pkt = rx_msg_info.is_last_pkt;

                    // All DATA pkts are 1 MTU long, except (possibly) the last one of a msg.
                    // NOTE: the following requires isPow2(MAX_SEG_LEN_BYTES).
                    bit<L2_MAX_SEG_LEN_BYTES> msg_len_mod_mtu = hdr.homa.msg_len[L2_MAX_SEG_LEN_BYTES-1:0];
                    bit<16> msg_len_pkts;
                    if (msg_len_mod_mtu == 0) {
                        msg_len_pkts = (hdr.homa.msg_len >> L2_MAX_SEG_LEN_BYTES);
                    } else {
                        msg_len_pkts = (hdr.homa.msg_len >> L2_MAX_SEG_LEN_BYTES) + 1;
                    }

                    pendingMsgInfo_t cur_msg_info;
                    cur_msg_info.src_ip         = hdr.ipv4.srcAddr;
                    cur_msg_info.src_context    = hdr.homa.src;
                    cur_msg_info.dst_context    = hdr.homa.dst;
                    cur_msg_info.tx_msg_id      = hdr.homa.tx_msg_id;
                    cur_msg_info.msg_len        = hdr.homa.msg_len;
                    cur_msg_info.buf_ptr        = hdr.homa.buf_ptr;
                    cur_msg_info.buf_size_class = hdr.homa.buf_size_class;
                    cur_msg_info.ackNo          = rx_msg_info.ackNo;
                    grantInfo_t cur_msg_grant_info;
                    cur_msg_grant_info.grantedIdx     = meta.params.rtt_pkts;
                    cur_msg_grant_info.grantableIdx   = meta.params.rtt_pkts + 1;
                    cur_msg_grant_info.remaining_size = (bit<8>)(msg_len_pkts - 1);
                    curMsgReg_req_t cur_msg_info_req;
                    cur_msg_info_req.index      = rx_msg_info.rx_msg_id;
                    cur_msg_info_req.msg_info   = cur_msg_info;
                    cur_msg_info_req.grant_info = cur_msg_grant_info;
                    cur_msg_info_req.is_new_msg = rx_msg_info.is_new_msg;
                    grantInfo_t updated_msg_grant_info;

                    curMsgReg.apply(cur_msg_info_req, updated_msg_grant_info);

                    bool is_fully_granted = updated_msg_grant_info.grantedIdx >= msg_len_pkts;

                    grantSched_req_t sched_req;
                    sched_req.rx_msg_id    = rx_msg_info.rx_msg_id;
                    sched_req.rank         = updated_msg_grant_info.remaining_size;
                    sched_req.removeObj    = is_fully_granted;
                    sched_req.grantableIdx = updated_msg_grant_info.grantableIdx;
                    sched_req.grantedIdx   = updated_msg_grant_info.grantedIdx;
                    grantSched_resp_t sched_resp;

                    grantScheduler.apply(sched_req, sched_resp);

                    if (sched_resp.success && sched_resp.prio_level < HOMA_OVERCOMMITMENT_LEVEL) {
                        // generate GRANT
                        grantMsgReg_req_t granted_msg_ingo_req;
                        granted_msg_ingo_req.index      = sched_resp.grant_msg_id;
                        granted_msg_ingo_req.grantedIdx = sched_resp.grant_offset;
                        pendingMsgInfo_t granted_msg_info;
                        grantMsgReg.apply(granted_msg_ingo_req, granted_msg_info);

                        ctrlPkt_meta.dst_ip         = granted_msg_info.src_ip;
                        ctrlPkt_meta.dst_context    = granted_msg_info.src_context;
                        ctrlPkt_meta.msg_len        = granted_msg_info.msg_len;
                        ctrlPkt_meta.pkt_offset     = granted_msg_info.ackNo;
                        ctrlPkt_meta.src_context    = granted_msg_info,dst_context;
                        ctrlPkt_meta.tx_msg_id      = granted_msg_info.tx_msg_id;
                        ctrlPkt_meta.buf_ptr        = granted_msg_info.buf_ptr;
                        ctrlPkt_meta.buf_size_class = granted_msg_info.buf_size_class;
                        ctrlPkt_meta.credit         = sched_resp.grant_offset;
                        ctrlPkt_meta.rank           = sched_resp.prio_level + HOMA_NUM_UNSCHEDULED_PRIOS;
                        ctrlPkt_meta.flags          = GRANT_MASK;

                        grantPkt_event.apply(ctrlPkt_meta, dummy);

                    } else {
                        // generate ACK for the current msg, rather than a GRANT
                        ctrlPkt_meta.pkt_offset = rx_msg_info.ackNo;
                        ctrlPkt_meta.credit = 0; // This is just an ACK, no packet is granted
                        ctrlPkt_meta.flags = ACK_MASK;

                        ackPkt_event.apply(ctrlPkt_meta, dummy);
                    }
                }
            } else {
                // Processing control pkt (ACK / NACK / GRANT)
                // do not pass ctrl pkts to assembly module
                smeta.drop = 1;

                bool is_ack = (hdr.homa.flags & ACK_MASK) > 0;
                bool is_nack = (hdr.homa.flags & NACK_MASK) > 0;
                bool is_grant = (hdr.homa.flags & GRANT_MASK) > 0;

                if (is_grant) {
                    // Update tx_msg_id's priority.
                    txMsgPrioReg_req_t prio_req;
                    prio_req.index  = hdr.homa.tx_msg_id;
                    prio_req.update = true;
                    prio_req.prio   = hdr.homa.grant_prio;
                    txMsgPrioReg_resp_t prio_resp;

                    txMsgPrioReg.apply(prio_req, prio_resp);
                }

                if (is_ack || is_nack || is_grant) {
                    bit<16> ackNo = is_nack ? hdr.homa.grant_offset : hdr.homa.pkt_offset;
                    // fire delivered event
                    delivered_meta_t delivered_meta;
                    delivered_meta.tx_msg_id      = hdr.homa.tx_msg_id;
                    delivered_meta.delivered_pkts = ((PktBitmap_t)1 << ackNo);
                    delivered_meta.msg_len        = hdr.homa.msg_len;
                    delivered_meta.buf_ptr        = hdr.homa.buf_ptr;
                    delivered_meta.buf_size_class = hdr.homa.buf_size_class;

                    delivered_event.apply(delivered_meta, dummy);
                }

                if (is_nack || is_grant) {
                    // fire creditToBtx event
                    creditToBtx_meta_t creditToBtx_meta;
                    creditToBtx_meta.tx_msg_id       = hdr.homa.tx_msg_id;
                    creditToBtx_meta.rtx             = is_nack;
                    creditToBtx_meta.rtx_pkt_offset  = hdr.homa.pkt_offset;
                    creditToBtx_meta.update_credit   = is_grant;
                    creditToBtx_meta.new_credit      = hdr.homa.grant_offset;
                    creditToBtx_meta.buf_ptr         = hdr.homa.buf_ptr;
                    creditToBtx_meta.buf_size_class  = hdr.homa.buf_size_class;
                    creditToBtx_meta.dst_ip          = hdr.ipv4.srcAddr;
                    creditToBtx_meta.dst_context     = hdr.homa.src;
                    creditToBtx_meta.msg_len         = hdr.homa.msg_len;
                    creditToBtx_meta.src_context     = hdr.homa.dst;

                    creditToBtx_event.apply(creditToBtx_meta, dummy);
                }
            }
        } else {
            // non-Homa packet from network
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
