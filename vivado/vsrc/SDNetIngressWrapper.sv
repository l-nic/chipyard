
// *************************************************************************
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
// *************************************************************************
// NOTE: machine-generated file --- DO NOT EDIT!!
// *************************************************************************
//`timescale 1ns/1ps

import sdnet_ingress_pkg::*;

module SDNetIngressWrapper #(
  parameter TDATA_W = 512
) (
  // Packet In
  input                     net_net_in_valid,
  output                    net_net_in_ready,
  input       [TDATA_W-1:0] net_net_in_bits_data,
  input   [(TDATA_W/8)-1:0] net_net_in_bits_keep,
  input                     net_net_in_bits_last,

  // Packet Out
  output                    net_net_out_valid,
  input                     net_net_out_ready,
  output      [TDATA_W-1:0] net_net_out_bits_data,
  output  [(TDATA_W/8)-1:0] net_net_out_bits_keep,
  output                    net_net_out_bits_last,

  // Extern call outputs
  output                    net_get_rx_msg_info_req_valid,
  output             [31:0] net_get_rx_msg_info_req_bits_src_ip,
  output             [15:0] net_get_rx_msg_info_req_bits_src_context,
  output             [15:0] net_get_rx_msg_info_req_bits_tx_msg_id,
  output             [15:0] net_get_rx_msg_info_req_bits_msg_len,

  // Extern call inputs
  input                     net_get_rx_msg_info_resp_valid,
  input                     net_get_rx_msg_info_resp_bits_fail,
  input              [15:0] net_get_rx_msg_info_resp_bits_rx_msg_id,

  // Metadata Out
  output                    net_meta_out_valid,
  output             [31:0] net_meta_out_src_ip,
  output             [15:0] net_meta_out_src_context,
  output             [15:0] net_meta_out_bits_msg_len,
  output              [7:0] net_meta_out_bits_pkt_offset,
  output             [15:0] net_meta_out_dst_context,
  output             [15:0] net_meta_out_rx_msg_id,

  input                     reset,
  input                     clock
);

  // AXI-Lite Control Signals
  // TODO(sibanez): connect to sim_control
  wire                    s_axil_awvalid;
  wire             [31:0] s_axil_awaddr;
  wire                    s_axil_awready;
  wire                    s_axil_wvalid;
  wire             [31:0] s_axil_wdata;
  wire                    s_axil_wready;
  wire                    s_axil_bvalid;
  wire              [1:0] s_axil_bresp;
  wire                    s_axil_bready;
  wire                    s_axil_arvalid;
  wire             [31:0] s_axil_araddr;
  wire                    s_axil_arready;
  wire                    s_axil_rvalid;
  wire             [31:0] s_axil_rdata;
  wire              [1:0] s_axil_rresp;
  wire                    s_axil_rready;

  reg                 user_metadata_in_valid;
  USER_META_DATA_T    user_metadata_in;
  wire                user_metadata_out_valid;
  USER_META_DATA_T    user_metadata_out;

  USER_EXTERN_VALID_T user_extern_out_valid;
  USER_EXTERN_OUT_T   user_extern_out;
  USER_EXTERN_VALID_T user_extern_in_valid;
  USER_EXTERN_IN_T    user_extern_in;

  assign net_meta_out_valid = user_metadata_out_valid;
  assign {net_meta_out_src_ip,
          net_meta_out_src_context,
          net_meta_out_bits_msg_len,
          net_meta_out_bits_pkt_offset,
          net_meta_out_dst_context,
          net_meta_out_rx_msg_id} = user_metadata_out;

  assign net_get_rx_msg_info_req_valid = user_extern_out_valid.get_rx_msg_info;
  assign {net_get_rx_msg_info_req_bits_src_ip,
          net_get_rx_msg_info_req_bits_src_context,
          net_get_rx_msg_info_req_bits_tx_msg_id,
          net_get_rx_msg_info_req_bits_msg_len} = user_extern_out.get_rx_msg_info;

  assign user_extern_in_valid.get_rx_msg_info = net_get_rx_msg_info_resp_valid;
  assign user_extern_in.get_rx_msg_info = {net_get_rx_msg_info_resp_bits_fail,
                                           net_get_rx_msg_info_resp_bits_rx_msg_id};

  // SDNet module
  sdnet_ingress sdnet_ingress_inst (
    // Clocks & Resets
    .s_axis_aclk             (clock),
    .s_axis_aresetn          (~reset),
    // NOTE: We will use the same clock for both AXI-Stream
    // and AXI-Lite. Usually, the AXI-Lite clock is much
    // slower, but since we'll run the clock @ ~100MHz, I
    // think this should be fine.
    // TODO(sibanez): Should check if Firesim is even able to simulate designs
    // with multiple clock domains.
    .s_axi_aclk              (clock),
    .s_axi_aresetn           (~reset),
    // Metadata
    .user_metadata_in        (user_metadata_in),
    .user_metadata_in_valid  (user_metadata_in_valid),
    .user_metadata_out       (user_metadata_out),
    .user_metadata_out_valid (user_metadata_out_valid),
    // User Extern Data
    .user_extern_in          (user_extern_in),
    .user_extern_in_valid    (user_extern_in_valid),
    .user_extern_out         (user_extern_out),
    .user_extern_out_valid   (user_extern_out_valid),
    // AXI4 Stream Slave port
    .s_axis_tvalid           (net_net_in_valid),
    .s_axis_tready           (net_net_in_ready),
    .s_axis_tdata            (net_net_in_bits_data),
    .s_axis_tkeep            (net_net_in_bits_keep),
    .s_axis_tlast            (net_net_in_bits_last),
    // AXI4 Stream Master port
    .m_axis_tvalid           (net_net_out_valid),
    .m_axis_tready           (net_net_out_ready),
    .m_axis_tdata            (net_net_out_bits_data),
    .m_axis_tkeep            (net_net_out_bits_keep),
    .m_axis_tlast            (net_net_out_bits_last),
     // Slave AXI-lite interface
    .s_axi_awaddr            (s_axil_awaddr),
    .s_axi_awvalid           (s_axil_awvalid),
    .s_axi_awready           (s_axil_awready),
    .s_axi_wdata             (s_axil_wdata),
    .s_axi_wstrb             (4'hF),
    .s_axi_wvalid            (s_axil_wvalid),
    .s_axi_wready            (s_axil_wready),
    .s_axi_bresp             (s_axil_bresp),
    .s_axi_bvalid            (s_axil_bvalid),
    .s_axi_bready            (s_axil_bready),
    .s_axi_araddr            (s_axil_araddr),
    .s_axi_arvalid           (s_axil_arvalid),
    .s_axi_arready           (s_axil_arready),
    .s_axi_rdata             (s_axil_rdata),
    .s_axi_rvalid            (s_axil_rvalid),
    .s_axi_rready            (s_axil_rready),
    .s_axi_rresp             (s_axil_rresp)
  );

  assign user_metadata_in = 'b0;

  // State machine to drive user_metadata_in_valid
  reg state, next_state;
  localparam START = 0;
  localparam WAIT_EOP = 1;

  always @(*) begin
    // defaults
    next_state = state;
    net_net_in_valid = 0;

    case (state)
      START: begin
        if (net_net_in_valid && net_net_in_ready) begin
          net_net_in_valid = 1;
          if (!net_net_in_bits_last) begin
            next_state = WAIT_EOP;
          end
        end
      end
      WAIT_EOP: begin
        if (net_net_in_valid && net_net_in_ready && net_net_in_bits_last) begin
          next_state = START;
        end
      end
    endcase
  end

  always @(posedge clock) begin
    if (reset) begin
      state <= START;
    end
    else begin
      state <= next_state;
    end
  end

endmodule: SDNetIngressWrapper
