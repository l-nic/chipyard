// ----------------------------------------------------------------------- //
//
// Copyright (c) 2020 Stanford University All rights reserved.
//
// This software was developed by
// Stanford University and the University of Cambridge Computer Laboratory
// under National Science Foundation under Grant No. CNS-0855268,
// the University of Cambridge Computer Laboratory under EPSRC INTERNET Project EP/H040536/1 and
// by the University of Cambridge Computer Laboratory under DARPA/AFRL contract FA8750-11-C-0249 ("MRC2"),
// as part of the DARPA MRC research programme.
//
// @NETFPGA_LICENSE_HEADER_START@
//
// Licensed to NetFPGA C.I.C. (NetFPGA) under one or more contributor
// license agreements.  See the NOTICE file distributed with this work for
// additional information regarding copyright ownership.  NetFPGA licenses this
// file to you under the NetFPGA Hardware-Software License, Version 1.0 (the
// "License"); you may not use this file except in compliance with the
// License.  You may obtain a copy of the License at:
//
//   http://www.netfpga-cic.org
//
// Unless required by applicable law or agreed to in writing, Work distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations under the License.
//
// @NETFPGA_LICENSE_HEADER_END@

#include <core.p4>
#include <xsa.p4>
#include "lnic_common.p4"

/* IO for get_rx_msg_info extern */
// Input
struct get_rx_msg_info_req_t {
    IPv4Addr_t  src_ip;
    ContextID_t src_context;
    MsgID_t     tx_msg_id;
    bit<16>     msg_len;
}
// Output
struct get_rx_msg_info_resp_t {
    bool fail;
    MsgID_t rx_msg_id;
    bool is_new_msg;
}

/* IO for delivered event */
struct delivered_meta_t {
  MsgID_t tx_msg_id;
  bit<8> pkt_offset;
  bit<16> msg_len;
  bit<16> buf_ptr;
  bit<8> buf_size_class;
}

/* IO for creditToBtx event */
struct creditToBtx_meta_t {
  MsgID_t tx_msg_id;
  bool rtx;
  bit<8> rtx_pkt_offset;
  bool update_credit;
  bit<16> new_credit;
  // Additional fields for generating pkts
  // NOTE: these could be stored in tables indexed by tx_msg_id, but this would require extra state ...
  bit<16> buf_ptr;
  bit<8> buf_size_class;
  IPv4Addr_t dst_ip;
  ContextID_t dst_context;
  bit<16> msg_len;
  ContextID_t src_context;
}

// dummy response for events
struct dummy_t {
  bit<1> unused;
}

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
                inout ingress_metadata meta, 
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
            LNIC_PROTO : parse_lnic;
            default    : accept; 
        }
    }
    
    state parse_lnic {
        packet.extract(hdr.lnic);
        transition accept;
    }

}

// ****************************************************************************** //
// **************************  P R O C E S S I N G   **************************** //
// ****************************************************************************** //

control MyProcessing(inout headers hdr, 
                     inout ingress_metadata meta, 
                     inout standard_metadata_t smeta) {

    UserExtern<get_rx_msg_info_req_t, get_rx_msg_info_resp_t>(2) get_rx_msg_info;
    UserExtern<delivered_meta_t, dummy_t>(1) delivered_event;
    UserExtern<creditToBtx_meta_t, dummy_t>(1) creditToBtx_event;
    UserExtern<egress_metadata, dummy_t>(1) ctrlPkt_event;

    // state used to compute pull offset
    // TODO(sibanez): set latency here after implementing the extern
    UserExtern<ifElseRaw_req_t, ifElseRaw_resp_t>(2) credit_ifElseRaw;

    apply {
        // Process all pkts arriving at the NIC
        if (hdr.lnic.isValid()) {
            dummy_t dummy; // unused metadata for events
            if (hdr.lnic.flags & DATA_MASK > 0) {

                // get info about msg from message assembly module
                get_rx_msg_info_req_t req;
                req.src_ip = hdr.ipv4.srcAddr;
                req.src_context = hdr.lnic.src_context;
                req.tx_msg_id = hdr.lnic.tx_msg_id;
                req.msg_len = hdr.lnic.msg_len;
                get_rx_msg_info_resp_t rx_msg_info;
                get_rx_msg_info.apply(req, rx_msg_info);

                bool genACK  = false; // default
                bool genNACK = false; // default
                bool genPULL = true;
                bit<16> pull_offset_diff = 0;
                if (hdr.lnic.flags & CHOP_MASK > 0) {
                    // DATA pkt has been chopped ==> send NACK
                    genNACK = true;
                    smeta.drop = 1;
                } else {
                    // DATA pkt is intact ==> send ACK
                    genACK = true;
                    pull_offset_diff = 1; // increase pull offset

                    // Fill out metadata for packets going to CPU
                    meta.src_ip = hdr.ipv4.srcAddr;
                    meta.src_context = hdr.lnic.src_context;
                    meta.msg_len = hdr.lnic.msg_len;
                    meta.pkt_offset = hdr.lnic.pkt_offset;
                    meta.dst_context = hdr.lnic.dst_context;
                    meta.rx_msg_id = rx_msg_info.rx_msg_id;
                    meta.tx_msg_id = hdr.lnic.tx_msg_id;
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
                    credit_req.data_0     = RTT_PKTS + pull_offset_diff;
                    credit_req.opCode_0   = REG_WRITE;
                    credit_req.predicate  = rx_msg_info.is_new_msg;
                    ifElseRaw_resp_t credit_resp;
                    credit_ifElseRaw.apply(credit_req, credit_resp);

                    bit<16> pull_offset = credit_resp.new_val;

                    // generate ctrl pkt(s)
                    egress_metadata ctrlPkt_meta;
                    ctrlPkt_meta.dst_ip         = hdr.ipv4.srcAddr;
                    ctrlPkt_meta.dst_context    = hdr.lnic.src_context;
                    ctrlPkt_meta.msg_len        = hdr.lnic.msg_len;
                    ctrlPkt_meta.pkt_offset     = hdr.lnic.pkt_offset;
                    ctrlPkt_meta.src_context    = hdr.lnic.dst_context;
                    ctrlPkt_meta.tx_msg_id      = hdr.lnic.tx_msg_id;
                    ctrlPkt_meta.buf_ptr        = hdr.lnic.buf_ptr;
                    ctrlPkt_meta.buf_size_class = hdr.lnic.buf_size_class;
                    ctrlPkt_meta.pull_offset    = pull_offset;
                    ctrlPkt_meta.genACK         = genACK;
                    ctrlPkt_meta.genNACK        = genNACK;
                    ctrlPkt_meta.genPULL        = genPULL;

                    ctrlPkt_event.apply(ctrlPkt_meta, dummy);
                }

                hdr.eth.setInvalid();
                hdr.ipv4.setInvalid();
                hdr.lnic.setInvalid();
            } else {
                // Processing control pkt (ACK / NACK / PULL)
                if (hdr.lnic.flags & ACK_MASK > 0) {
                    // fire delivered event
                    delivered_meta_t delivered_meta;
                    delivered_meta.tx_msg_id      = hdr.lnic.tx_msg_id;
                    delivered_meta.pkt_offset     = hdr.lnic.pkt_offset;
                    delivered_meta.msg_len        = hdr.lnic.msg_len;
                    delivered_meta.buf_ptr        = hdr.lnic.buf_ptr;
                    delivered_meta.buf_size_class = hdr.lnic.buf_size_class;

                    delivered_event.apply(delivered_meta, dummy);
                }
                if (hdr.lnic.flags & NACK_MASK > 0 || hdr.lnic.flags & PULL_MASK > 0) {
                    bool rtx = (hdr.lnic.flags & NACK_MASK > 0);
                    bool update_credit = (hdr.lnic.flags & PULL_MASK > 0);

                    // fire creditToBtx event
                    creditToBtx_meta_t creditToBtx_meta;
                    creditToBtx_meta.tx_msg_id       = hdr.lnic.tx_msg_id;
                    creditToBtx_meta.rtx             = rtx;
                    creditToBtx_meta.rtx_pkt_offset  = hdr.lnic.pkt_offset;
                    creditToBtx_meta.update_credit   = update_credit;
                    creditToBtx_meta.new_credit      = hdr.lnic.pull_offset;
                    creditToBtx_meta.buf_ptr         = hdr.lnic.buf_ptr;
                    creditToBtx_meta.buf_size_class  = hdr.lnic.buf_size_class;
                    creditToBtx_meta.dst_ip          = hdr.ipv4.srcAddr;
                    creditToBtx_meta.dst_context     = hdr.lnic.src_context;
                    creditToBtx_meta.msg_len         = hdr.lnic.msg_len;
                    creditToBtx_meta.src_context     = hdr.lnic.dst_context;

                    creditToBtx_event.apply(creditToBtx_meta, dummy);
                }
                // do not pass ctrl pkts to assembly module
                smeta.drop = 1;
            }
        } else {
            // non-LNIC packet from network
            smeta.drop = 1;
        }
    }
} 

// ****************************************************************************** //
// ***************************  D E P A R S E R  ******************************** //
// ****************************************************************************** //

control MyDeparser(packet_out packet, 
                   in headers hdr,
                   inout ingress_metadata meta, 
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
