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
