#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "lnic.h"

#define REPEAT_3(X) X X X
#define REPEAT_4(X) X X X X
#define REPEAT_8(X) REPEAT_4(X) REPEAT_4(X)
#define REPEAT_15(X) REPEAT_8(X) REPEAT_4(X) X X X
#define REPEAT_16(X) REPEAT_8(X) REPEAT_8(X)
#define REPEAT_32(X) REPEAT_16(X) REPEAT_16(X)
#define REPEAT_63(X) REPEAT_32(X) REPEAT_16(X) REPEAT_8(X) REPEAT_4(X) X X X
#define REPEAT_64(X) REPEAT_32(X) REPEAT_32(X)
#define REPEAT_127(X) REPEAT_64(X) REPEAT_32(X) REPEAT_16(X) REPEAT_8(X) REPEAT_4(X) X X X
#define REPEAT_128(X) REPEAT_64(X) REPEAT_64(X)

/* Copy all except the last word back into the network.
 */
void copy_payload(uint16_t msg_len) {

  if (msg_len == 8) {
    return;
  } else if (msg_len == 32) {
    REPEAT_3(lnic_copy();)
  } else if (msg_len == 128) {
    REPEAT_15(lnic_copy();)
  } else if (msg_len == 512) {
    REPEAT_63(lnic_copy();)
  } else if (msg_len == 1024) {
    REPEAT_127(lnic_copy();)
  } else {
    printf("Unsupported msg len: %d\n", msg_len);
  }
}

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
  copy_payload(msg_len);
  start_time = lnic_read();
  lnic_write_r(start_time);
  lnic_msg_done();

  // forward all subsequent pkts and insert start timestamp
  while (1) {
    lnic_wait();
    app_hdr = lnic_read();
    lnic_write_r(app_hdr);
    msg_len = (uint16_t)app_hdr;
    copy_payload(msg_len);
    lnic_read(); // discard current pkt's timestamp
    lnic_write_r(start_time);
    lnic_msg_done();
  }
  return 0;
}

