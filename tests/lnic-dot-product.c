#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "lnic.h"
#include "dot-product.h"

// If DOT_PROD_OPT is not defined then the whole msg will be copied
// from the register file to memory before computing the dot product.
#define DOT_PROD_OPT
#define MAX_MSG_WORDS 258

#define RESP_MSG_LEN 32

/* Dot Product:
 * - Compute the dot product of each msg with the in-memory data.
 */

int main(void)
{
  uint64_t app_hdr;
  uint64_t start_time;
  uint64_t num_msgs;
  int msg_cnt;
  int configured;
  int i;
  uint64_t start_misses, num_misses;
 
  uint64_t msg_words[MAX_MSG_WORDS];

  // register context ID with L-NIC
  uint64_t context_id = 0;
  uint64_t priority = 0;
  lnic_add_context(context_id, priority);

  // Setup mhpmcounter3 performance counter to count D$ misses
  write_csr(mhpmevent3, 0x202);

  printf("Initializing...\n");
  // Initialize the working set
  uint64_t weights[NUM_WEIGHTS];
  for (i = 0; i < NUM_WEIGHTS; i++) {
    weights[i] = i;
  }

  printf("Ready!\n");
  while (1) {
    start_misses = read_csr(mhpmcounter3);
    msg_cnt = 0;
    configured = 0;
    // Wait for a Config msg, which indicates how many msgs to process
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

      // write app_hdr and msg type for response
      lnic_write_r((app_hdr & (IP_MASK | CONTEXT_MASK)) | RESP_MSG_LEN);
      lnic_write_i(RESP_TYPE);

      if (lnic_read() != DATA_TYPE) {
        printf("Expected Data msg.\n");
        return -1;
      }

      // Compute the dot product of the msg with the in-memory data.
      // Fetch the specified in-memory data and multiply the data with
      // the msg word. Accumulate for all msg words.
      uint64_t num_words = lnic_read();
      uint64_t result = 0;
#ifdef DOT_PROD_OPT
      for (i = 0; i < num_words; i++) {
        uint64_t word = lnic_read();
        result += word * weights[word];
      }
#else
      for (i = 0; i < num_words; i++) {
        msg_words[i] = lnic_read();
      }

      for (i = 0; i < num_words; i++) {
        result += msg_words[i] * weights[msg_words[i]];
      }
#endif
      lnic_write_r(result);
      // write cache misses into response
      num_misses = read_csr(mhpmcounter3) - start_misses;
      lnic_write_r(num_misses);

      // discard timestamp on RX msg
      lnic_read();
      // write start_time into response
      lnic_write_r(start_time);

      lnic_msg_done();
      msg_cnt++;
    }
  }
  return 0;
}

