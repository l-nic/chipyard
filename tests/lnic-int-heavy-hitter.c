#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "lnic.h"

// Number of reports (per-core / per-context)
#define NUM_REPORTS 2

#define UPSTREAM_IP 0x0a010100
// Use this dst port to have the HW compute latency
#define LATENCY_PORT 0x1234

#define MAX_NUM_FLOWS 256
// max allowable flow rate (Gbps)
#define FLOW_RATE_THRESH 10

#define START_FLAG_MASK 0x2
#define FIN_FLAG_MASK 0x4

#define DONE_TYPE 0
#define HH_EVENT_TYPE 1

#define HH_EVENT_MSG_LEN 32
#define DONE_MSG_LEN 16

/* State */

typedef struct {
  bool valid;
  uint64_t ip_info;
  uint64_t port_info;
  uint64_t start_time;
  uint64_t byte_count;
} flow_state_t;

void process_msgs() {
  uint64_t tx_app_hdr;
  int i;

  bool first_recvd = false;
  uint64_t start_time;

  // initialize state
  int total_report_count = 0;
  flow_state_t flowState[MAX_NUM_FLOWS];
//  for (i = 0; i < MAX_NUM_FLOWS; i++) {
//    flowState[i].valid = false;
//  }

  printf("Initialization complete!\n");

  while (1) {
    // wait for a report to arrive
    lnic_wait();
    // read application hdr
    lnic_read();

    // word 1 - flow src/dst IP
    uint64_t ip_info = lnic_read();
    // word 2 - flow src/dst port and ip_proto
    uint64_t port_info = lnic_read();
    // NOTE: we will use the flow dst_port as the flow hash for now ...
    uint16_t flow_hash = (port_info & 0xffff00000000) >> 32;
    // word 3 - flow flags
    uint64_t flow_flags = lnic_read();
    bool is_start = (flow_flags & START_FLAG_MASK) > 0;
    bool is_fin = (flow_flags & FIN_FLAG_MASK) > 0;
    // word 4 - report timestamp (ns)
    uint64_t report_timestamp = lnic_read();

//    printf("ip_info=%lx, port_info=%lx, flow_hash=%d, flow_flags=%lx, is_start=%d, is_fin=%d report_timestamp=%lx\n", ip_info, port_info, flow_hash, flow_flags, is_start, is_fin, report_timestamp);

    // update state
//    if (flowState[flow_hash].valid && ((flowState[flow_hash].ip_info != ip_info) || (flowState[flow_hash].port_info != port_info))) {
//      printf("ERROR: flow hash collision!\n");
//      return;
//    }

    if (is_start) {
      flowState[flow_hash].byte_count = 0;
      flowState[flow_hash].start_time = report_timestamp;
    }

    flowState[flow_hash].valid = true;
    flowState[flow_hash].ip_info = ip_info;
    flowState[flow_hash].port_info = port_info;
    // word 5 - pkt length
    flowState[flow_hash].byte_count += lnic_read();

    // word 6
    uint64_t ingress_switch_ip = lnic_read();

    // word 7
    uint64_t nic_timestamp = lnic_read();

    // compute flow rate (Gbps)
    uint64_t flow_rate = (is_start) ? 0 : flowState[flow_hash].byte_count * 8 / (report_timestamp - flowState[flow_hash].start_time);

    // Generate heavy hitter event if needed
    if (flow_rate > FLOW_RATE_THRESH) {
      tx_app_hdr = (ingress_switch_ip << 32)
                   | (LATENCY_PORT << 16)
                   | HH_EVENT_MSG_LEN;
      lnic_write_r(tx_app_hdr);
      lnic_write_i(HH_EVENT_TYPE);
      lnic_write_r(flowState[flow_hash].ip_info);
      lnic_write_r(flowState[flow_hash].port_info);
      lnic_write_r(nic_timestamp);
    }

    // free flow state when flow completes
    if (is_fin) {
      flowState[flow_hash].byte_count = 0;
      flowState[flow_hash].valid = false;
    }

    if (!first_recvd) {
      start_time = nic_timestamp;
      first_recvd = true;
    }

    total_report_count++;
    // Check if all reports have been processed
    if (total_report_count >= NUM_REPORTS) {
      // send DONE msg
//      printf("Sending DONE msg!\n");
      uint64_t upstream_ip = UPSTREAM_IP;
      tx_app_hdr = (upstream_ip << 32)
                   | (LATENCY_PORT << 16)
                   | 16;
      lnic_write_r(tx_app_hdr);
      lnic_write_i(DONE_TYPE);
      lnic_write_r(start_time);
      // reset state
      total_report_count = 0;
    }

    lnic_msg_done();
  }
}

bool is_single_core() { return false; }

int core_main(int argc, char** argv, int cid, int nc) {
  // Each core runs a different context
  uint64_t context_id = cid;
  uint64_t priority = 0;

  lnic_add_context(context_id, priority);

  process_msgs();

  return 0;
}

