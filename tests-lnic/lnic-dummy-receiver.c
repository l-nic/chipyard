#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "lnic.h"

int main(void)
{
  uint64_t app_hdr;
  uint16_t msg_len;
  uint16_t num_words;
  
  // register context ID with L-NIC
  uint64_t context_id = 0;
  uint64_t priority = 0;
  lnic_add_context(context_id, priority);

  printf("Ready!\n");
  while (1) {
    // wait for a pkt to arrive
    lnic_wait();
    // read request application hdr
    app_hdr = lnic_read();
    // extract msg_len and compute number of words in the msg
    msg_len = (uint16_t)app_hdr;
    num_words = msg_len/LNIC_WORD_SIZE;
    if (msg_len % LNIC_WORD_SIZE != 0) { num_words++; }
    // read msg words then move onto the next msg
    for (int i = 0; i < num_words; i++) {
      lnic_read();
    }
    lnic_msg_done();
  }
  return 0;
}

