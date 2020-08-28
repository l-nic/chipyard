#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "lnic.h"

// This program assumes that the LNIC HW has been generated using
// MAX_SEG_LEN_BYTES = 64
#define MAX_SEG_LEN_BYTES 64

#define UPSTREAM_COLLECTOR_IP 0x0a000006
#define UPSTREAM_COLLECTOR_PORT 0x1111
// Use this dst port to have the HW compute latency
#define LATENCY_PORT 0x1234

// This is the number of switches that each packet goes through
#define NUM_HOPS 3

// This is the number of pkts sent through the switches.
// Hence the number of raw postcards = NUM_PKTS * NUM_HOPS
#define NUM_PKTS 20

/* Optimized INT Collector:
 *   - Aggregate postcards using LNIC msg reassembly layer
 *   - Compute the total delay for each packet
 */

#define RAW_POSTCARD_LEN NUM_HOPS*MAX_SEG_LEN_BYTES
/* Input postcard message format:
 *   - app_hdr: srcIP, srcPort, RAW_POSTCARD_LEN
 *   One of these 64B payloads for each hop:
 *   - word 1: dstIP, dstPort, txMsgID
 *   - word 2: pktOffset
 *   - word 3: packet queueing time
 *   - word 4: unused
 *   - word 5: unused
 *   - word 6: unused
 *   - word 7: unused
 *   - word 8: NIC ingress timestamp
 */

#define AGG_POSTCARD_LEN 24
/* Output aggregated postcard message format:
 *   - word 0: UPSTREAM_COLLECTOR_IP, UPSTREAM_COLLECTOR_PORT, AGG_POSTCARD_LEN
 *   - word 1: dstIP, dstPort, txMsgID
 *   - word 2: srcIP, srcPort, pktOffset
 *   - word 3: total packet queueing time
 */

/* Output DONE msg format:
 *   - word 0: UPSTREAM_COLLECTOR_IP, LATENCY_PORT, msgLen
 *   - word 1: NIC timestamp of first raw postcard
 */

void process_msgs() {
  uint64_t rx_app_hdr;
  uint64_t tx_app_hdr;
  uint64_t word1;
  uint64_t pkt_offset;
  uint64_t ts;
  uint16_t msg_len;
  int i;

  bool first_recvd = false;
  uint64_t start_time;

  // initialize state
  int total_card_count = 0;

  while (1) {
    // wait for a postcard to arrive
    lnic_wait();
    // read postcard application hdr
    rx_app_hdr = lnic_read();
    // check msg len in app hdr
    msg_len = (uint16_t)rx_app_hdr;
    if (msg_len != RAW_POSTCARD_LEN) {
      printf("ERROR: expected msg_len = %d, actual msg_len = %d\n", RAW_POSTCARD_LEN, msg_len);
      return -1;
    } 

    uint64_t total_qtime = 0;

    // process first postcard in message
    word1 = lnic_read();
    pkt_offset = lnic_read();
    total_qtime += lnic_read();
//    printf("total_qtime = %ld\n", total_qtime);
    lnic_read();
    lnic_read();
    lnic_read();
    lnic_read();
    // read NIC timestamp of first postcard
    ts = lnic_read();
    if (!first_recvd) {
      start_time = ts;
      first_recvd = true;
    }

    // process second postcard in message
    lnic_read();
    lnic_read();
    total_qtime += lnic_read();
//    printf("total_qtime = %ld\n", total_qtime);
    lnic_read();
    lnic_read();
    lnic_read();
    lnic_read();
    lnic_read();

    // process third postcard in message
    lnic_read();
    lnic_read();
    total_qtime += lnic_read();
//    printf("total_qtime = %ld\n", total_qtime);
    lnic_read();
    lnic_read();
    lnic_read();
    lnic_read();
    lnic_read();

    // update state
    total_card_count += 1;

    // send aggregated postcard
    tx_app_hdr = (UPSTREAM_COLLECTOR_IP << 32)
                 | (UPSTREAM_COLLECTOR_PORT << 16)
                 | AGG_POSTCARD_LEN;
    lnic_write_r(tx_app_hdr);
    lnic_write_r(word1);
    uint32_t src_ip = rx_app_hdr >> 32;
    uint16_t src_port = (rx_app_hdr & CONTEXT_MASK) >> 16;
    lnic_write_r((src_ip << 32) | (src_port << 16) | (0xffff & pkt_offset));
    lnic_write_r(total_qtime);

    // Check if all postcards have been received for all packets
    if (total_card_count >= NUM_PKTS) {
      // send DONE msg
      tx_app_hdr = (UPSTREAM_COLLECTOR_IP << 32)
                   | (LATENCY_PORT << 16)
                   | 8;
      lnic_write_r(tx_app_hdr);
      lnic_write_r(start_time);
      // reset state
      total_card_count = 0;
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

