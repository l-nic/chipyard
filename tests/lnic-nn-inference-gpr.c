#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "lnic.h"

/**
 * NN inference nanoservice implementation on RISC-V using LNIC GPR implementation.
 *
 * NN hdr format:
 *   bit<64> type - 0 = Config, 1 = Weight, 2 = Data
 *
 * Config Msg Format:
 *   bit<64> num_edges
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

#define MAX_NUM_EDGES 128
#define CONFIG_TYPE 0
#define WEIGHT_TYPE 1
#define DATA_TYPE 2

int main(void) {
  // send initial boot msg
  lnic_boot();

  // allocate stack memory for weights
  uint64_t weights[MAX_NUM_EDGES];
  int i;
  uint64_t index;
  uint64_t app_hdr;
  uint64_t msg_type;
  int edge_cnt;
  uint64_t num_edges;
  uint64_t result;
  char configured;

  while(1) {
    edge_cnt = 0;
    result = 0;
    // wait for Config msg
    configured = 0;
    while (!configured) {
      lnic_wait();
      lnic_read(); // discard app hdr
      // TODO(sibanez): this is not the ideal way to branch because lnic_read()
      //   copies the result into a GPR first. Should really use a single branch inst
      if (lnic_read() == CONFIG_TYPE) {
        num_edges = lnic_read();
	configured = 1;
      } else {
	// discard msg
        lnic_read();
	lnic_read();
      }
    }

    // process weight and data msgs
    while (edge_cnt < num_edges) {
      lnic_wait();
      app_hdr = lnic_read();
      msg_type = lnic_read();
      index = lnic_read();
      if (msg_type == WEIGHT_TYPE) {
        weights[index] = lnic_read();
      } else if (msg_type == DATA_TYPE) {
        result += weights[index] * lnic_read();
	edge_cnt++;
      }
    }

    // send out result
    lnic_write_r(app_hdr); // write app hdr
    lnic_write_i(DATA_TYPE); // index
    lnic_write_i(0); // index
    lnic_write_r(result);
  }
  return 0;
}

