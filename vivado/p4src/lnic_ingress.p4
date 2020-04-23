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

// Output metadata
struct metadata {
    IPv4Addr_t   src_ip;
    ContextID_t  src_context;
    bit<16>      msg_len;
    bit<8>       pkt_offset;
    ContextID_t  dst_context;
    MsgID_t      rx_msg_id;
}

// Request metadata to extern call
struct get_rx_msg_info_req_t {
    IPv4Addr_t  src_ip;
    ContextID_t src_context;
    MsgID_t     tx_msg_id;
    bit<16>     msg_len;
}

// Response metadata from extern call
struct get_rx_msg_info_resp_t {
    bit<1> fail;
    MsgID_t rx_msg_id;
    // TODO(sibanez): add additional fields for transport processing
}

// ****************************************************************************** //
// *************************** P A R S E R  ************************************* //
// ****************************************************************************** //

parser MyParser(packet_in packet, 
                out headers hdr, 
                inout metadata meta, 
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
                     inout metadata meta, 
                     inout standard_metadata_t smeta) {

    UserExtern<get_rx_msg_info_req_t, get_rx_msg_info_resp_t>(2) get_rx_msg_info;

    apply {
        // process pkts going from network to CPU
        if (hdr.lnic.isValid()) {
            // get info about msg from message assembly module
            get_rx_msg_info_req_t req;
            req.src_ip = hdr.ipv4.srcAddr;
            req.src_context = hdr.lnic.src_context;
            req.tx_msg_id = hdr.lnic.tx_msg_id;
            req.msg_len = hdr.lnic.msg_len;
            get_rx_msg_info_resp_t resp;
            get_rx_msg_info.apply(req, resp);

            // Fill out metadata for packets going to CPU
            meta.src_ip = hdr.ipv4.srcAddr;
            meta.src_context = hdr.lnic.src_context;
            meta.msg_len = hdr.lnic.msg_len;
            meta.pkt_offset = hdr.lnic.pkt_offset;
            meta.dst_context = hdr.lnic.dst_context;
            meta.rx_msg_id = resp.rx_msg_id;
            if (resp.fail == 1) {
                // drop pkt if failed to allocate a receive buffer for the msg
                smeta.drop = 1;
            }

            hdr.eth.setInvalid();
            hdr.ipv4.setInvalid();
            hdr.lnic.setInvalid();
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
                   inout metadata meta, 
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
