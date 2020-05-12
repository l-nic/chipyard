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

typedef bit<48>  EthAddr_t;
typedef bit<32>  IPv4Addr_t;
typedef bit<16>  ContextID_t;
typedef bit<16>  MsgID_t;

const EthAddr_t SWITCH_MAC_ADDR = 0x085566778808;
const EthAddr_t NIC_MAC_ADDR = 0x081122334408;
const IPv4Addr_t NIC_IP_ADDR = 0x0A000001;

const bit<16> IP_HDR_BYTES = 20;

// NOTE: these must be updated if the LNIC header format changes
const bit<16> LNIC_HDR_BYTES = 15;
const bit<16> LNIC_CTRL_PKT_BYTES = 79;

const bit<16> IPV4_TYPE = 0x0800;
const bit<8> LNIC_PROTO = 0x99;

// NOTE: this must match up with LNIC.scala
const bit<16> RTT_PKTS = 5;

// ******************************************************************************* //
// *************************** M E T A D A T A *********************************** //
// ******************************************************************************* //

struct ingress_metadata {
    IPv4Addr_t   src_ip;
    ContextID_t  src_context;
    bit<16>      msg_len;
    bit<8>       pkt_offset;
    ContextID_t  dst_context;
    MsgID_t      rx_msg_id;
}

struct egress_metadata {
    IPv4Addr_t  dst_ip;
    ContextID_t dst_context;
    bit<16>     msg_len;
    bit<8>      pkt_offset;
    ContextID_t src_context;
    MsgID_t     tx_msg_id;
    bit<16>     buf_ptr;
    bit<8>      buf_size_class;
    bit<16>     pull_offset;
    bool        genACK;
    bool        genNACK;
    bool        genPULL;
}

// ****************************************************************************** //
// *************************** H E A D E R S  *********************************** //
// ****************************************************************************** //

// standard Ethernet header (14 bytes = 112 bits)
header eth_mac_t {
    EthAddr_t dstAddr;
    EthAddr_t srcAddr;
    bit<16> etherType;
}

// IPv4 header without options (20 bytes = 160 bits)
header ipv4_t {
    bit<4> version;
    bit<4> ihl;
    bit<8> tos;
    bit<16> totalLen;
    bit<16> identification;
    bit<3> flags;
    bit<13> fragOffset;
    bit<8> ttl;
    bit<8> protocol;
    bit<16> hdrChecksum;
    IPv4Addr_t srcAddr;
    IPv4Addr_t dstAddr;
}

const bit<8> DATA_MASK = 0b00000001;
const bit<8> ACK_MASK  = 0b00000010;
const bit<8> NACK_MASK = 0b00000100;
const bit<8> PULL_MASK = 0b00001000;
const bit<8> CHOP_MASK = 0b00010000;
// L-NIC transport header
header lnic_t {
    bit<8> flags;
    ContextID_t src_context;
    ContextID_t dst_context;
    bit<16> msg_len;
    bit<8> pkt_offset;
    bit<16> pull_offset;
    MsgID_t tx_msg_id;
    bit<16> buf_ptr;
    bit<8> buf_size_class;
}

// header structure
struct headers {
    eth_mac_t  eth;
    ipv4_t     ipv4;
    lnic_t     lnic;
}


