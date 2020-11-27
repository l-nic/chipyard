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
  // register context ID with L-NIC
  lnic_add_context(0, 0);

  // allocate stack memory for weights
  uint64_t weights[MAX_NUM_EDGES];
  uint64_t index;
  uint64_t app_hdr;
  uint64_t msg_type;
  int edge_cnt;
  uint64_t num_edges;
  uint64_t result;
  uint64_t start_time;
  uint64_t config_type = CONFIG_TYPE;
  uint64_t weight_type = WEIGHT_TYPE;

  printf("Ready!\n");
  while(1) {
    edge_cnt = 0;
    result = 0;
configure:
    // wait for Config msg
    lnic_wait();
    lnic_read(); // discard app hdr
    // branch based on msg_type
    lnic_branch("bne", config_type, discard_pkt);
    num_edges = lnic_read();
    start_time = lnic_read();
    lnic_msg_done();
    //printf("Configured: num_edges = %lu\n", num_edges);
    goto process;
discard_pkt:
    // discard msg
    lnic_read();
    lnic_read();
    lnic_read();
    lnic_msg_done();
    goto configure;

process:
    // process weight and data msgs
    while (edge_cnt < num_edges) {
      lnic_wait();
      app_hdr = lnic_read();
      // branch based on msg_type
      lnic_branch("bne", weight_type, process_data);
      index = lnic_read();
      weights[index] = lnic_read();
      goto discard_timestamp;
      //printf("Weight[%lu] received.\n", index);
process_data:
      index = lnic_read();
      result += weights[index] * lnic_read();
      edge_cnt++;
      //printf("Data[%lu] received.\n\tedge_cnt = %lu\n\tresult = %lu\n", index, edge_cnt, result);
discard_timestamp:
      lnic_read(); // discard timestamp
      lnic_msg_done();
    }

    // send out result
    //printf("Sending out result = %lu");
    lnic_write_r(app_hdr); // write app hdr
    lnic_write_i(DATA_TYPE); // index
    lnic_write_i(0); // index
    lnic_write_r(result);
    lnic_write_r(start_time);
  }
  return 0;
}

