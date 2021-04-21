// ----------------------------------------------------------------------- //
// homa_egress.p4
// ----------------------------------------------------------------------- //

#include <core.p4>
#include <xsa.p4>
#include "lnic.p4"
#include "homa.p4"

// ****************************************************************************** //
// *************************** EXTERN INTERFACES ******************************** //
// ****************************************************************************** //

struct txMsgPrioReg_req_t {
    MsgID_t index;
    bool update;
    bit<8> prio;
}

struct txMsgPrioReg_resp_t {
    bit<8> prio;
}

// ****************************************************************************** //
// *************************** P A R S E R  ************************************* //
// ****************************************************************************** //

parser MyParser(packet_in packet, 
                out headers hdr, 
                inout full_egress_metadata_t meta, 
                inout standard_metadata_t smeta) {

    state start {
        transition accept;
    }
}

// ****************************************************************************** //
// **************************  P R O C E S S I N G   **************************** //
// ****************************************************************************** //

control MyProcessing(inout headers hdr, 
                     inout full_egress_metadata_t meta, 
                     inout standard_metadata_t smeta) {

    // Externs
    UserExtern<txMsgPrioReg_req_t, txMsgPrioReg_resp_t>(2) txMsgPrioReg;

    apply {
        bool is_data_pkt = (meta.meta.flags & DATA_MASK) > 0;

        // packet from CPU
        hdr.eth.setValid();
        hdr.ipv4.setValid();
        hdr.homa.setValid();

        // Fill out Ethernet header fields
        hdr.eth.dstAddr = meta.params.switch_mac_addr;
        hdr.eth.srcAddr = meta.params.nic_mac_addr;
        hdr.eth.etherType = IPV4_TYPE;

        // Fill out IPv4 header fields
        hdr.ipv4.version = 4;
        hdr.ipv4.ihl = 5;
        hdr.ipv4.tos = 0;
        if (!is_data_pkt) {
            hdr.ipv4.totalLen = IP_HDR_BYTES + LNIC_CTRL_PKT_BYTES;
        } else {
            // All DATA pkts are 1 MTU long, except (possibly) the last one of a msg.
            // NOTE: the following requires isPow2(MAX_SEG_LEN_BYTES).
            bit<L2_MAX_SEG_LEN_BYTES> msg_len_mod_mtu = meta.meta.msg_len[L2_MAX_SEG_LEN_BYTES-1:0];
            bit<16> msg_len_pkts;
            bit<16> last_bytes;
            if (msg_len_mod_mtu == 0) {
              msg_len_pkts = (meta.meta.msg_len >> L2_MAX_SEG_LEN_BYTES);
              last_bytes = MAX_SEG_LEN_BYTES;
            } else {
              msg_len_pkts = (meta.meta.msg_len >> L2_MAX_SEG_LEN_BYTES) + 1;
              last_bytes = msg_len_mod_mtu;
            }
            bool is_last_pkt = (meta.meta.pkt_offset == (msg_len_pkts - 1));

            hdr.ipv4.totalLen = is_last_pkt ?
                (last_bytes + IP_HDR_BYTES + LNIC_HDR_BYTES) :
                (MAX_SEG_LEN_BYTES + IP_HDR_BYTES + LNIC_HDR_BYTES);
        }
        hdr.ipv4.identification = 1;
        hdr.ipv4.flags = 0;
        hdr.ipv4.fragOffset = 0;
        hdr.ipv4.ttl = 64;
        hdr.ipv4.protocol = HOMA_PROTO;
        hdr.ipv4.hdrChecksum = 0; // TODO(sibanez): implement this ...
        hdr.ipv4.srcAddr = meta.params.nic_ip_addr;
        hdr.ipv4.dstAddr = meta.meta.dst_ip;

        // Fill out Homa header fields
        hdr.homa.flags          = meta.meta.meta.flags;
        hdr.homa.src            = meta.meta.src_context;
        hdr.homa.dst            = meta.meta.dst_context;
        hdr.homa.msg_len        = meta.meta.msg_len;
        hdr.homa.pkt_offset     = meta.meta.pkt_offset;
        hdr.homa.grant_offset   = meta.meta.grant_offset;
        hdr.homa.grant_prio     = 
        hdr.homa.tx_msg_id      = meta.meta.tx_msg_id;
        hdr.homa.buf_ptr        = meta.meta.buf_ptr;
        hdr.homa.buf_size_class = meta.meta.buf_size_class;
        hdr.homa.padding        = 0;
    }
} 

// ****************************************************************************** //
// ***************************  D E P A R S E R  ******************************** //
// ****************************************************************************** //

control MyDeparser(packet_out packet, 
                   in headers hdr,
                   inout full_egress_metadata_t meta, 
                   inout standard_metadata_t smeta) {
    apply {
        packet.emit(hdr.eth);
        packet.emit(hdr.ipv4);
        packet.emit(hdr.homa);
    }
}

// ****************************************************************************** //
// *******************************  M A I N  ************************************ //
// ****************************************************************************** //

XilinxPipeline(
    MyParser(), 
    MyProcessing(), 
    MyDeparser()
) main;
