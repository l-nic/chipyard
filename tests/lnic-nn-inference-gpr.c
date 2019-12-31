#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "lnic.h"

/**
 * NN inference nanoservice implementation on RISC-V using LNIC GPR implementation.
 *
 * Two types of msgs:
 *   (1) Weight - indicates weight to use to scale data on the specified edge
 *   (2) Data - data arriving on the specified edge, also used to send response
 *
 * Weight Msg Format:
 *   bit<64> index
 *   bit<64> weight
 *
 * Data Msg Format:
 *   bit<64> index
 *   bit<64> data
 *
 */

#define NUM_EDGES 3

int main(void) {
  // send initial boot msg
  lnic_boot();

  // allocate stack memory for weights
  uint64_t weights[NUM_EDGES];
  int i;
  uint64_t index;
  uint64_t app_hdr;

  // receive weights
  for (i = 0; i < NUM_EDGES; i++) {
    lnic_wait();
    app_hdr = lnic_read(); // read and discard app hdr
    index = lnic_read();
    weights[index] = lnic_read();
  }

  // receive data
  uint64_t result = 0;
  for (i = 0; i < NUM_EDGES; i++) {
    lnic_wait();
    app_hdr = lnic_read(); // read app hdr
    index = lnic_read();
    result += weights[index] * lnic_read();
  }

  // send out result
  lnic_write_r(app_hdr); // write app hdr
  lnic_write_i(0); // index
  lnic_write_r(result);

  return 0;
}

