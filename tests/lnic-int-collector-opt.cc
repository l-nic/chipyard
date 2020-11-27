#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "mica/util/hash.h"
#include "lnic.h"

#define NUM_REPORTS 2

#define UPSTREAM_COLLECTOR_IP 0x0a000006
#define UPSTREAM_COLLECTOR_PORT 0x1111
// Use this dst port to have the HW compute latency
#define LATENCY_PORT 0x1234

#define MAX_NUM_HOPS 8
#define MAX_NUM_FLOWS 256
#define MAX_NUM_QUEUES 256
#define MAX_NUM_LINKS 256

#define SWID_MASK 0x80
#define L1_PORT_MASK 0x40
#define HOP_LATENCY_MASK 0x20
#define Q_MASK 0x10
#define INGRESS_TS_MASK 0x08
#define EGRESS_TS_MASK 0x04
#define L2_PORT_MASK 0x02
#define UTILIZATION_MASK 0x01

#define BASE_PATH_EVENT_LEN 32
#define PATH_LATENCY_EVENT_LEN 40
#define HOP_LATENCY_EVENT_LEN 40
#define QSIZE_EVENT_LEN 32
#define LINK_UTIL_EVENT_LEN 32

#define PATH_EVENT_TYPE 0
#define PATH_LATENCY_EVENT_TYPE 1
#define HOP_LATENCY_EVENT_TYPE 2
#define QSIZE_EVENT_TYPE 3
#define LINK_UTIL_EVENT_TYPE 4

#define MAX_PROF_SAMPLES 128
#define PROFILE_POINT \
    if (total_report_count == 1) { \
      prof_cycles[prof_count] = rdcycle(); \
      prof_count++; \
    }

/* INT Collector:
 *   - Receive INT reports and perform event detection
 *   - When an event is detected, send an event message upstream
 */

template <typename T>
static uint64_t mica_hash(const T *key, size_t key_length) {
  return ::mica::util::hash(key, key_length);
}

typedef struct {
  uint32_t src_ip;
  uint32_t dst_ip;
  uint16_t src_port;
  uint16_t dst_port;
} flow_key_t;

bool operator!=(const flow_key_t& lhs, const flow_key_t& rhs)
{
    return (lhs.src_ip != rhs.src_ip) || (lhs.dst_ip != rhs.dst_ip) || (lhs.src_port != rhs.src_port) || (lhs.dst_port != rhs.dst_port);
}

typedef struct {
  uint32_t swID;
  uint16_t qID;
} q_key_t;

bool operator!=(const q_key_t& lhs, const q_key_t& rhs)
{
    return (lhs.swID != rhs.swID) || (lhs.qID != rhs.qID);
}

typedef struct {
  uint32_t swID;
  uint16_t portID;
} link_key_t;

bool operator!=(const link_key_t& lhs, const link_key_t& rhs)
{
    return (lhs.swID != rhs.swID) || (lhs.portID != rhs.portID);
}

/* State */

typedef struct {
  bool valid;
  flow_key_t flow_key;
  int num_hops;
  uint32_t path[MAX_NUM_HOPS];
  uint64_t path_latency;
  uint64_t hop_latency[MAX_NUM_HOPS];
} flow_state_t;

typedef struct {
  bool valid;
  q_key_t q_key;
  uint32_t q_size;
} q_state_t;

typedef struct {
  bool valid;
  link_key_t link_key;
  uint32_t tx_utilization;
} link_state_t;

#define PROCESS_HOP_META swID = lnic_read(); \
  path_change |= (flowState[flow_hash].path[i] != swID); \
  flowState[flow_hash].path[i] = swID;
//  msg_word = lnic_read(); \
//  l1_ingress_port = (msg_word & 0xffff0000) >> 16; \
//  l1_egress_port = msg_word & 0xffff; \
//  link_key.swID = swID; \
//  link_key.portID = l1_egress_port; \
//  link_hash = swID; \
//  hop_latency = lnic_read(); \
//  path_latency += hop_latency; \
//  hop_latency_change = (flowState[flow_hash].hop_latency[i] != hop_latency); \
//  flowState[flow_hash].hop_latency[i] = hop_latency; \
//  if (is_new_flow || hop_latency_change) { \
//    tx_app_hdr = (upstream_collector_ip << 32) \
//                 | (UPSTREAM_COLLECTOR_PORT << 16) \
//                 | HOP_LATENCY_EVENT_LEN; \
//    lnic_write_r(tx_app_hdr); \
//    lnic_write_i(HOP_LATENCY_EVENT_TYPE); \
//    lnic_write_r(report_timestamp); \
//    tx_msg_word = flow_key.src_ip; \
//    lnic_write_r( (tx_msg_word << 32) | flow_key.dst_ip); \
//    tx_msg_word = flow_key.src_port; \
//    tx_msg_word = (tx_msg_word << 16) | flow_key.dst_port; \
//    tx_msg_word = (tx_msg_word << 32) | swID; \
//    lnic_write_r(tx_msg_word); \
//    lnic_write_r(hop_latency); \
//  } \
//  msg_word = lnic_read(); \
//  qID = (msg_word & 0xff000000) >> 24; \
//  q_size = msg_word & 0xffffff; \
//  q_key.swID = swID; \
//  q_key.qID = qID; \
//  q_hash = swID; \
//  if (qState[q_hash].valid && (qState[q_hash].q_key != q_key)) { \
//    return; \
//  } else if (!qState[q_hash].valid || (qState[q_hash].q_size != q_size)) { \
//    qState[q_hash].valid = true; \
//    qState[q_hash].q_key = q_key; \
//    qState[q_hash].q_size = q_size; \
//    tx_app_hdr = (upstream_collector_ip << 32) \
//                 | (UPSTREAM_COLLECTOR_PORT << 16) \
//                 | QSIZE_EVENT_LEN; \
//    lnic_write_r(tx_app_hdr); \
//    lnic_write_i(QSIZE_EVENT_TYPE); \
//    lnic_write_r(report_timestamp); \
//    tx_msg_word = swID; \
//    lnic_write_r((tx_msg_word << 32) | qID); \
//    lnic_write_r(q_size); \
//  } \
//  ingress_timestamp = lnic_read(); \
//  egress_timestamp = lnic_read(); \
//  msg_word = lnic_read(); \
//  l2_ingress_port = (msg_word & 0xffff0000) >> 16; \
//  l2_egress_port = msg_word & 0xffff; \
//  tx_utilization = lnic_read(); \
//  if (linkState[link_hash].valid && (linkState[link_hash].link_key != link_key)) { \
//    return; \
//  } else if (!linkState[link_hash].valid || (linkState[link_hash].tx_utilization != tx_utilization)) { \
//    linkState[link_hash].valid = true; \
//    linkState[link_hash].link_key = link_key; \
//    linkState[link_hash].tx_utilization = tx_utilization; \
//    tx_app_hdr = (upstream_collector_ip << 32) \
//                 | (UPSTREAM_COLLECTOR_PORT << 16) \
//                 | LINK_UTIL_EVENT_LEN; \
//    lnic_write_r(tx_app_hdr); \
//    lnic_write_i(LINK_UTIL_EVENT_TYPE); \
//    lnic_write_r(report_timestamp); \
//    tx_msg_word = swID; \
//    lnic_write_r((tx_msg_word << 32) | l1_egress_port); \
//    lnic_write_r(tx_utilization); \
//  }

void process_msgs() {
  uint64_t upstream_collector_ip = UPSTREAM_COLLECTOR_IP;

  uint64_t tx_app_hdr;
  uint64_t msg_word;
  uint64_t nic_ts;
  uint64_t tx_msg_word;
  int i;

  flow_key_t flow_key;

  uint32_t report_timestamp;
  uint32_t src_ip;
  uint32_t dst_ip;
  uint16_t src_port;
  uint16_t dst_port;
  uint8_t int_hdr_len;
  uint8_t hopMLen;
  uint8_t int_ins;

  uint32_t swID;
  uint16_t l1_ingress_port;
  uint16_t l1_egress_port;
  uint32_t hop_latency;
  uint8_t qID;
  uint32_t q_size;
  uint32_t ingress_timestamp;
  uint32_t egress_timestamp;
  uint16_t l2_ingress_port;
  uint16_t l2_egress_port;
  uint32_t tx_utilization;

  link_key_t link_key;
  uint8_t link_hash;
  q_key_t q_key;
  uint8_t q_hash;

  int num_hops;

  bool first_recvd = false;
  uint64_t start_time;

  // TODO(sibanez): profiling code
  uint64_t prof_cycles[MAX_PROF_SAMPLES];
  int prof_count = 0;

  // initialize state
  int total_report_count = 0;

  flow_state_t flowState[MAX_NUM_FLOWS];
  q_state_t qState[MAX_NUM_QUEUES];
  link_state_t linkState[MAX_NUM_LINKS];

  for (i = 0; i < MAX_NUM_FLOWS; i++) {
    flowState[i].valid = false;
  }

  for (i = 0; i < MAX_NUM_QUEUES; i++) {
    qState[i].valid = false;
  }

  for (i = 0; i < MAX_NUM_LINKS; i++) {
    linkState[i].valid = false;
  }

  printf("Initialization complete!\n");

  while (1) {
    // wait for a report to arrive
    lnic_wait();

    PROFILE_POINT

    // process app header
    lnic_read();
    // process INT report
    // word 1
    msg_word = lnic_read();
    // word 2 - report_timestamp[31:16]
    report_timestamp = (lnic_read() & 0xffff) << 16;
    // word 3 - report_timestamp[15:0]
    report_timestamp |= (lnic_read() & 0xffff000000000000) >> 48;
    // words 4 & 5
    lnic_read();
    lnic_read();
    // word 6 - src_ip
    src_ip = (uint32_t)lnic_read();
    // word 7 - dst_ip, src_port, dst_port
    msg_word = lnic_read();
    dst_ip = msg_word >> 32;
    src_port = (msg_word & 0xffff0000) >> 16;
    dst_port = msg_word & 0xffff;
    // word 8 - INT header length
    int_hdr_len = (lnic_read() & 0xff00) >> 8;
    // word 9 - hopMLen, INT instruction
    msg_word = lnic_read();
    hopMLen = (msg_word & 0xff0000000000) >> 40;
    int_ins = (msg_word & 0xff000000) >> 24;

//    printf("report_timestamp = %d\nsrc_ip = %x\ndst_ip = %x\nsrc_port = %d\ndst_port = %d\nint_hdr_len = %d\nhopMLen = %d\nint_ins = %x\n",
//            report_timestamp, src_ip, dst_ip, src_port, dst_port, int_hdr_len, hopMLen, int_ins);

    // Compute flow_hash - NOTE: currently just using the dst_port. Should be a hash of the 5-tuple eventually
    flow_key.src_ip = src_ip;
    flow_key.dst_ip = dst_ip;
    flow_key.src_port = src_port;
    flow_key.dst_port = dst_port;
    uint8_t flow_hash = dst_port; // mica_hash(&flow_key, sizeof(flow_key));

    // Detect flow hash collisions
    if (flowState[flow_hash].valid && (flowState[flow_hash].flow_key != flow_key)) {
      printf("ERROR: hash collision on flow state!\n");
      return;
    }
    bool is_new_flow = !flowState[flow_hash].valid;
    // insert flow
    flowState[flow_hash].valid = true;
    flowState[flow_hash].flow_key = flow_key;

    bool path_change = false;
    uint64_t path_latency = 0;

    bool hop_latency_change;

    num_hops = (hopMLen > 0) ? (int_hdr_len - 3)/hopMLen : 0;

    PROFILE_POINT

    // Unroll INT metadata processing - assume all 8 fields are active
    i = 0;
    PROCESS_HOP_META
    PROFILE_POINT
//    i = 1;
//    PROCESS_HOP_META
//    PROFILE_POINT
//    i = 2;
//    PROCESS_HOP_META
//    PROFILE_POINT
//    i = 3;
//    PROCESS_HOP_META
//    PROFILE_POINT
//    i = 4;
//    PROCESS_HOP_META
//    PROFILE_POINT
//    i = 5;
//    PROCESS_HOP_META
//    PROFILE_POINT

    // fire path event if needed
    if (is_new_flow || path_change) {
      tx_app_hdr = (upstream_collector_ip << 32)
                   | (UPSTREAM_COLLECTOR_PORT << 16)
                   | (BASE_PATH_EVENT_LEN + num_hops*8);
      lnic_write_r(tx_app_hdr);
      lnic_write_i(PATH_EVENT_TYPE);
      lnic_write_r(report_timestamp);
      tx_msg_word = src_ip;
      lnic_write_r( (tx_msg_word << 32) | dst_ip);
      tx_msg_word = src_port;
      tx_msg_word = (tx_msg_word << 16) | dst_port;
      tx_msg_word = (tx_msg_word << 32) | num_hops;
      lnic_write_r(tx_msg_word);
      for (i = 0; i < num_hops; i++) {
        lnic_write_r(flowState[flow_hash].path[i]);
      }
    }

    bool has_hop_latency = int_ins & HOP_LATENCY_MASK;

    // detect path latency changes and fire PathLatency Event
    bool path_latency_change = has_hop_latency && (flowState[flow_hash].path_latency != path_latency);
    flowState[flow_hash].path_latency = path_latency;
    if (is_new_flow || path_latency_change) {
      tx_app_hdr = (upstream_collector_ip << 32)
                   | (UPSTREAM_COLLECTOR_PORT << 16)
                   | PATH_LATENCY_EVENT_LEN;
      lnic_write_r(tx_app_hdr);
      lnic_write_i(PATH_LATENCY_EVENT_TYPE);
      lnic_write_r(report_timestamp);
      tx_msg_word = src_ip;
      lnic_write_r( (tx_msg_word << 32) | dst_ip);
      tx_msg_word = src_port;
      tx_msg_word = (tx_msg_word << 16) | dst_port;
      tx_msg_word = tx_msg_word << 32;
      lnic_write_r(tx_msg_word);
      lnic_write_r(path_latency);
    }

    // NOTE: this assumes that the metadata words are 64-bit aligned.
    // read NIC timestamp
    nic_ts = lnic_read();
//    printf("nic_ts = %ld\n", nic_ts);

    // read NIC timestamp
    if (!first_recvd) {
      start_time = nic_ts;
      first_recvd = true;
    }

    PROFILE_POINT

    // TODO(sibanez): print profile results
    if (total_report_count == 1) {
      printf("Profile Results:\n");
      for (i = 0 ; i < prof_count-1; i++) {
        printf("Points %d -> %d: %ld cycles\n", i, i+1, prof_cycles[i+1] - prof_cycles[i]);
      }
    }

    total_report_count++;
    // Check if all reports have been processed
    if (total_report_count >= NUM_REPORTS) {
      // send DONE msg
      tx_app_hdr = (upstream_collector_ip << 32)
                   | (LATENCY_PORT << 16)
                   | 8;
      lnic_write_r(tx_app_hdr);
      lnic_write_r(start_time);
      // reset state
      total_report_count = 0;
    }

    lnic_msg_done();
  }
}

extern "C" {

// Only use core 0
int main(int argc, char** argv) {
  uint64_t context_id = 0;
  uint64_t priority = 0;
  lnic_add_context(context_id, priority);

  process_msgs();

  return 0;
}

}
