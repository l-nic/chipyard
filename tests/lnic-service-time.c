#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "lnic.h"
#include "service-time.h"
#include "lnic-loopback.h"

int main(void)
{
  uint64_t app_hdr;
  uint16_t msg_len;
  uint64_t start_time;
  uint64_t service_time;
  uint64_t num_msgs;
  int msg_cnt;
  int configured;
 
  // register context ID with L-NIC
  uint64_t context_id = 0;
  uint64_t priority = 0;
  lnic_add_context(context_id, priority);

  printf("Ready!\n");
  while (1) {
    msg_cnt = 0;
    configured = 0;
    // Wait for a Config msg
    while (!configured) {
      lnic_wait();
      lnic_read(); // discard app hdr
      if (lnic_read() != CONFIG_TYPE) {
        printf("Expected Config msg.\n");
        return -1;
      }
      num_msgs = lnic_read();
      start_time = lnic_read();
      lnic_msg_done();
      configured = 1;
    }
    // Process all requests
    while (msg_cnt < num_msgs) {
      lnic_wait();
      // extract msg len from app hdr
      app_hdr = lnic_read();
      msg_len = (uint16_t)app_hdr;
      lnic_write_r(app_hdr);

      lnic_copy(); // msg type
      service_time = lnic_read();

      // write msg
      lnic_write_r(service_time);

      // copy over msg payload
      copy_payload(msg_len - 16);
      // discard timestamp
      lnic_read();

      // perform the indicated amount of processing
      process_msg(service_time);

      lnic_write_r(start_time);

      lnic_msg_done();
      msg_cnt++;
    }
  }
  return 0;
}

