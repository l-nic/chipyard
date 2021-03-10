#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "lnic.h"
#include "lnic-loopback.h"

int main(void)
{
  uint64_t app_hdr;
  uint16_t msg_len;
  
  // register context ID with L-NIC
  uint64_t context_id = 0;
  uint64_t priority = 0;
  lnic_add_context(context_id, priority);

  printf("Ready!\n");
  // Send boot pkt to tell test to start
  lnic_boot_msg();
  while (1) {
    // wait for a pkt to arrive
    lnic_wait();
    // read request application hdr
    app_hdr = lnic_read();
    // write response application hdr
    lnic_write_r(app_hdr);
    // extract msg_len
    msg_len = (uint16_t)app_hdr;
    // copy over msg type, payload, and timestamp
    lnic_copy(); // msg type, unused
    copy_payload(msg_len - 16);
    lnic_copy();
    lnic_msg_done();
  }
  return 0;
}

