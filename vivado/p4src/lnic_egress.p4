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

// Input metadata
struct metadata {
    IPv4Addr_t  dst_ip;
    ContextID_t dst_context;
    bit<16>     msg_len;
    bit<8>      pkt_offset;
    ContextID_t src_context;
    MsgID_t     tx_msg_id;
    bit<16>     buf_ptr;
    bit<8>      buf_size_class;
}

// ****************************************************************************** //
// *************************** P A R S E R  ************************************* //
// ****************************************************************************** //

parser MyParser(packet_in packet, 
                out headers hdr, 
                inout metadata meta, 
                inout standard_metadata_t smeta) {

    state start {
        transition accept;
    }

}

// ****************************************************************************** //
// **************************  P R O C E S S I N G   **************************** //
// ****************************************************************************** //

control MyProcessing(inout headers hdr, 
                     inout metadata meta, 
                     inout standard_metadata_t smeta) {

    apply {
        // packet from CPU
        hdr.eth.setValid();
        hdr.ipv4.setValid();
        hdr.lnic.setValid();

        // Fill out Ethernet header fields
        hdr.eth.dstAddr = SWITCH_MAC_ADDR;
        hdr.eth.srcAddr = NIC_MAC_ADDR;
        hdr.eth.etherType = IPV4_TYPE;

        // Fill out IPv4 header fields
        hdr.ipv4.version = 4;
        hdr.ipv4.ihl = 5;
        hdr.ipv4.tos = 0;
        hdr.ipv4.totalLen = meta.msg_len + IP_HDR_BYTES + LNIC_HDR_BYTES;
        hdr.ipv4.identification = 1;
        hdr.ipv4.flags = 0;
        hdr.ipv4.fragOffset = 0;
        hdr.ipv4.ttl = 64;
        hdr.ipv4.protocol = LNIC_PROTO;
        hdr.ipv4.hdrChecksum = 0; // TODO(sibanez): implement this ...
        hdr.ipv4.srcAddr = NIC_IP_ADDR;
        hdr.ipv4.dstAddr = meta.dst_ip;

        // Fill out LNIC header fields
        hdr.lnic.src_context = meta.src_context;
        hdr.lnic.dst_context = meta.dst_context;
        hdr.lnic.msg_len = meta.msg_len;
        hdr.lnic.pkt_offset = meta.pkt_offset;
        hdr.lnic.tx_msg_id = meta.tx_msg_id;
        hdr.lnic.buf_ptr = meta.buf_ptr;
        hdr.lnic.buf_size_class = meta.buf_size_class;
    }
} 

// ****************************************************************************** //
// ***************************  D E P A R S E R  ******************************** //
// ****************************************************************************** //

control MyDeparser(packet_out packet, 
                   in headers hdr,
                   inout metadata meta, 
                   inout standard_metadata_t smeta) {
    apply {
        packet.emit(hdr.eth);
        packet.emit(hdr.ipv4);
        packet.emit(hdr.lnic);
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
