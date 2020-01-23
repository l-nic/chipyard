#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "lnic.h"

#define START_RX_TYPE 0
#define START_TX_TYPE 1
#define DATA_TYPE 2
#define DONE_TYPE 3

#define DONE_MSG_LEN 16

int main(void)
{
  uint64_t app_hdr;
  uint16_t msg_len;
  int num_words;
  int i, j;
  uint64_t msg_type;
  uint64_t start_time;
  uint64_t num_msgs;
  uint64_t msg_size;

  // register context ID with L-NIC
  lnic_add_context(0, 0);

  while (1) {
    // wait for a START RX or TX msg to arrive
    lnic_wait();
    app_hdr = lnic_read();
    if (lnic_read() == START_RX_TYPE) {
      // perform RX throughput test
      num_msgs = lnic_read(); // # of msgs to receive
      start_time = lnic_read();
      // read all msgs as fast as possible
      for (i = 0; i < num_msgs; i++) {
        lnic_wait();
	app_hdr = lnic_read();
        // extract msg_len
        msg_len = (uint16_t)app_hdr;
        num_words = msg_len/LNIC_WORD_SIZE;
        if (msg_len % LNIC_WORD_SIZE != 0) { num_words++; }
        // read and discard all msg data
        for (j = 0; j < num_words; j++) {
	  lnic_read();
        }
      }
      // send DONE msg
      lnic_write_r((app_hdr & (IP_MASK | CONTEXT_MASK)) | DONE_MSG_LEN); // app hdr
      lnic_write_i(DONE_TYPE); // msg type
      lnic_write_r(start_time); // timestamp
    } else {
      // perform TX throughput test
      num_msgs = lnic_read(); // # of msgs to generate
      msg_size = lnic_read(); // size of msgs to generate (bytes)
      start_time = lnic_read();
      // generate all required msgs
      for (i = 0; i < num_msgs; i++) {
        lnic_write_r((app_hdr & (IP_MASK | CONTEXT_MASK)) | msg_size);
	lnic_write_i(DATA_TYPE); // msg_type
        num_words = msg_size/LNIC_WORD_SIZE;
	if (msg_size % LNIC_WORD_SIZE != 0) { num_words++; }
	for (j = 0; j < num_words-2; j++) {
          lnic_write_i(0); // dummy data to generate
	}
	lnic_write_r(start_time); // timestamp
      }
    }
  }
  return 0;
}

