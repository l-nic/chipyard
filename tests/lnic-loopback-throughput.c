#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "lnic.h"
#include "lnic-loopback.h"

int main(void)
{
  printf("Ready!\n");
  uint64_t app_hdr;
  uint16_t msg_len;
  uint64_t start_time;
 
  // register context ID with L-NIC
  uint64_t context_id = 0;
  uint64_t priority = 0;
  lnic_add_context(context_id, priority);

  // forward 1st pkt and extract timestamp
  lnic_wait();
  app_hdr = lnic_read();
  lnic_write_r(app_hdr);
  msg_len = (uint16_t)app_hdr;
  copy_payload(msg_len - 8);
  start_time = lnic_read();
  lnic_write_r(start_time);
  lnic_msg_done();

  // forward all subsequent pkts and insert start timestamp
  while (1) {
    lnic_wait();
    app_hdr = lnic_read();
    lnic_write_r(app_hdr);
    msg_len = (uint16_t)app_hdr;
    copy_payload(msg_len - 8);
    lnic_read(); // discard current pkt's timestamp
    lnic_write_r(start_time);
    lnic_msg_done();
  }
  return 0;
}

