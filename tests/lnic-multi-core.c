#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "lnic.h"

void process_msgs() {
  uint64_t app_hdr;
  uint16_t msg_len;
  int num_words;
  int i;
  uint64_t stall_duration;

  while (1) {
    // wait for a pkt to arrive
    lnic_wait();
    // read request application hdr
    app_hdr = lnic_read();
    // write response application hdr
    lnic_write_r(app_hdr);
    // extract msg_len
    msg_len = (uint16_t)app_hdr;
//    printf("Received msg of length: %hu bytes", msg_len);
    num_words = msg_len/LNIC_WORD_SIZE;
    if (msg_len % LNIC_WORD_SIZE != 0) { num_words++; }
    // copy msg words back into network
    stall_duration = lnic_read();
    for (i = 0; i < stall_duration; i++) {
      asm volatile("nop");
    }
    lnic_write_r(stall_duration);
    for (i = 1; i < num_words; i++) {
      lnic_copy();
    }
    lnic_msg_done();
  }
}

void core0_main() {
  uint64_t context_id = 0;
  uint64_t priority = 0;
  lnic_add_context(context_id, priority);

  process_msgs();
}

void core1_main() {
  uint64_t context_id = 0;
  uint64_t priority = 0;
  lnic_add_context(context_id, priority);

  process_msgs();
}
