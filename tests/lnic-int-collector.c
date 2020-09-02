#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "lnic.h"

#define UPSTREAM_COLLECTOR_IP 0x0a000006
#define UPSTREAM_COLLECTOR_PORT 0x1111
// Use this dst port to have the HW compute latency
#define LATENCY_PORT 0x1234

#define MAX_NUM_HOPS 8
#define MAX_NUM_FLOWS 10
#define MAX_NUM_QUEUES 10
#define MAX_NUM_LINKS 10

#define SWID_MASK 0x80
#define L1_PORT_MASK 0x40
#define HOP_LATENCY_MASK 0x20
#define Q_MASK 0x10
#define INGRESS_TS_MASK 0x08
#define EGRESS_TS_MASK 0x04
#define L2_PORT_MASK 0x02
#define UTILIZATION_MASK 0x01

/* INT Collector:
 *   - Receive INT reports and perform event detection
 *   - When an event is detected, send an event message upstream
 */

/* State */

typedef struct {
  bool valid;
  uint64_t flowID;
  int num_hops;
  int path[MAX_NUM_HOPS];
  uint64_t path_latency;
  uint64_t hop_latency[MAX_NUM_HOPS];
} flow_state_t;

typedef struct {
  bool valid;
  int swID;
  uint16_t qID;
  int qsize;
} q_state_t;

typedef struct {
  bool valid;
  int swID;
  uint16_t portID;
  int utilization;
} link_state_t;

uint32_t get_next_word(uint64_t *msg_word, int *rem_words) {
  uint32_t next_word = (*msg_word) >> 32;
  (*rem_words)--;
  if ((*rem_words) > 0) {
    *msg_word = (*msg_word) << 32;
  } else {
    *msg_word = lnic_read();
    *rem_words = 2;
  }
  return next_word;
}

void process_msgs() {
  uint64_t rx_app_hdr;
  uint64_t tx_app_hdr;
  uint64_t msg_word;
  uint16_t msg_len;
  uint64_t nic_ts;
  int i;

  uint32_t report_timestamp;
  uint32_t src_ip;
  uint32_t dst_ip;
  uint16_t src_port;
  uint16_t dst_port;
  uint8_t int_hdr_len;
  uint8_t hopMLen;
  uint8_t int_ins;

  int num_hops;
  // buffers to store INT metadata words
  uint32_t meta_words[8*MAX_NUM_HOPS];
  int num_meta_words;

  uint32_t meta_word;
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

  bool first_recvd = false;
  uint64_t start_time;

  // initialize state
  int total_report_count = 0;

  flow_state_t flowState[MAX_NUM_FLOWS];
  for (i = 0; i < MAX_NUM_FLOWS; i++) {
    flowState[i].valid = false;
  }

  q_state_t qState[MAX_NUM_QUEUES];
  for (i = 0; i < MAX_NUM_QUEUES; i++) {
    qState[i].valid = false;
  }

  link_state_t linkState[MAX_NUM_LINKS];
  for (i = 0; i < MAX_NUM_LINKS; i++) {
    linkState[i].valid = false;
  }

  while (1) {
    // wait for a report to arrive
    lnic_wait();
    // process app header
    rx_app_hdr = lnic_read();
    msg_len = (uint16_t)rx_app_hdr; 

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

    printf("report_timestamp = %d\nsrc_ip = %x\ndst_ip = %x\nsrc_port = %d\ndst_port = %d\nint_hdr_len = %d\nhopMLen = %d\nint_ins = %x\n",
           report_timestamp, src_ip, dst_ip, src_port, dst_port, int_hdr_len, hopMLen, int_ins);

    num_hops = (int_hdr_len - 3)/hopMLen;
    msg_word = lnic_read();
    int rem_words = 2; // # of words remaining in the current msg_word
    for (i = 0; i < num_hops; i++) {
      printf("--- Hop %d ---\n", i);
      if (int_ins & SWID_MASK) {
        swID = get_next_word(&msg_word, &rem_words);
        printf("swID = %d\n", swID);
      }
      if (int_ins & L1_PORT_MASK) {
        meta_word = get_next_word(&msg_word, &rem_words);
        l1_ingress_port = (meta_word & 0xffff0000) >> 16;
        l1_egress_port = meta_word & 0xffff;
        printf("l1_ingress_port = %d\nl1_egress_port = %d\n", l1_ingress_port, l1_egress_port);
      }
      if (int_ins & HOP_LATENCY_MASK) {
        hop_latency = get_next_word(&msg_word, &rem_words);
        printf("hop_latency = %d\n", hop_latency);
      }
      if (int_ins & Q_MASK) {
        meta_word = get_next_word(&msg_word, &rem_words);
        qID = (meta_word & 0xff000000) >> 24;
        q_size = meta_word & 0xffffff;
        printf("qID = %d\nq_size = %d\n", qID, q_size);
      }
      if (int_ins & INGRESS_TS_MASK) {
        ingress_timestamp = get_next_word(&msg_word, &rem_words);
        printf("ingress_timestamp = %d\n", ingress_timestamp);
      }
      if (int_ins & EGRESS_TS_MASK) {
        egress_timestamp = get_next_word(&msg_word, &rem_words);
        printf("egress_timestamp = %d\n", egress_timestamp);
      }
      if (int_ins & L2_PORT_MASK) {
        meta_word = get_next_word(&msg_word, &rem_words);
        l2_ingress_port = (meta_word & 0xffff0000) >> 16;
        l2_egress_port = meta_word & 0xffff;
        printf("l2_ingress_port = %d\nl2_egress_port = %d\n", l2_ingress_port, l2_egress_port);
      }
      if (int_ins & UTILIZATION_MASK) {
        tx_utilization = get_next_word(&msg_word, &rem_words);
        printf("tx_utilization = %d\n", tx_utilization);
      }

    }

    // NOTE: this assumes that the metadata words are 64-bit aligned.
    // read NIC timestamp
    nic_ts = msg_word;
    printf("nic_ts = %ld\n", nic_ts);

    // read NIC timestamp
    if (!first_recvd) {
      start_time = nic_ts;
      first_recvd = true;
    }

    lnic_msg_done();
  }
}

// Only use core 0
int main(uint64_t argc, char** argv) {
  uint64_t context_id = 0;
  uint64_t priority = 0;
  lnic_add_context(context_id, priority);

  process_msgs();

  return 0;
}

