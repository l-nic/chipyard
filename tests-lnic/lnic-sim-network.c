#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "lnic.h"

#define NIC_IP 0x0a000002
#define NUM_MSGS 1
#define NUM_MSG_WORDS 4096

int send_recv() {
  uint64_t app_hdr;
  uint64_t dst_ip;
  uint64_t dst_context;
  int i;
  int n;

//  dst_ip = NIC_IP;
  dst_ip = 0x02;

  // Send one msg to each core
  for (n = 0; n < NUM_MSGS; n++) {
    dst_context = 0;
    app_hdr = (dst_ip << 32) | (dst_context << 16) | (NUM_MSG_WORDS*8);
    //printf("Sending message\n");
    lnic_write_r(app_hdr);
    for (i = 0; i < NUM_MSG_WORDS; i++) {
        lnic_write_r(i);
    }
  }
  
  // Receive all msgs
  for (n = 0; n < NUM_MSGS; n++) {
    // wait for a pkt to arrive
    lnic_wait();
    // read request application hdr
    app_hdr = lnic_read();
    // Check src IP
    uint64_t src_ip = (app_hdr & IP_MASK) >> 32;
    if (src_ip != NIC_IP) {
        printf("ERROR: Expected: correct_sender_ip = %x, Received: src_ip = %lx\n", NIC_IP, src_ip);
        return -1;
    }
    // Check src context
    uint64_t src_context = (app_hdr & CONTEXT_MASK) >> 16;
    if (src_context != 0) {
        printf("ERROR: Received: src_context = %ld\n", src_context);
        return -1;
    }
    // Check msg length
    uint16_t rx_msg_len = app_hdr & LEN_MASK;
    if (rx_msg_len != NUM_MSG_WORDS*8) {
        printf("ERROR: Expected: msg_len = %d, Received: msg_len = %d\n", NUM_MSG_WORDS*8, rx_msg_len);
        return -1;
    }
    // Check msg data
    for (i = 0; i < NUM_MSG_WORDS; i++) {
        uint64_t data = lnic_read();
        if (i != data) {
            printf("ERROR: Expected: data = %x, Received: data = %lx\n", i, data);
            return -1;
        }
    }
    lnic_msg_done();
  }

  // make sure RX queue is empty
  if (lnic_ready()) {
    printf("ERROR: RX Queue is not empty after processing all msgs!\n");
    return -1;
  }

  return 0;
}

int main(void)
{
  // register context ID with L-NIC
  uint64_t context_id = 0;
  uint64_t priority = 0;
  lnic_add_context(context_id, priority);
  printf("Ready!\n");

  int ret = send_recv();
  return ret;
}

