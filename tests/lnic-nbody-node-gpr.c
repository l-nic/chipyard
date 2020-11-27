#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "lnic.h"
#include "nbody.h"

/**
 * N-Body Simulation Node nanoservice implementation on RISC-V using LNIC GPR implementation.
 *
 * NN hdr format:
 *   bit<64> msg_type - 0 = Config, 1 = TraversalReq, 2 = TraversalResp
 *
 * Config Msg Format:
 *   bit<64> xcom
 *   bit<64> ycom
 *   bit<64> num_msgs
 *   bit<64> timestamp
 *
 * TraversalReq Format:
 *   bit<64> xpos
 *   bit<64> ypos
 *   bit<64> timestamp
 *
 * TraversalResp Format:
 *   bit<64> force
 *   bit<64> timestamp
 *
 */

int main(void) {
  // register context ID with L-NIC
  lnic_add_context(0, 0);

  // local variables
  uint64_t app_hdr;
  uint64_t msg_type;
  uint64_t xcom = 0;
  uint64_t ycom = 0;
  uint64_t xpos, ypos;
  int msg_cnt;
  uint64_t num_msgs;
  uint64_t force;
  uint64_t start_time;
  int configured;

  printf("Ready!\n");
  while(1) {
    msg_cnt = 0;
    configured = 0;
    // wait for a Config msg
    while (!configured) {
      lnic_wait();
      lnic_read(); // discard app hdr
      if (lnic_read() != CONFIG_TYPE) {
	printf("Expected Config msg.\n");
        return -1;
      }
      xcom = lnic_read();
      ycom = lnic_read();
      num_msgs = lnic_read();
      start_time = lnic_read();
      lnic_msg_done();
      configured = 1;
    }
    // Process all requests and send one response at the end
    while (msg_cnt < num_msgs) {
      lnic_wait();
      app_hdr = lnic_read(); // read app hdr
      if (lnic_read() != TRAVERSAL_REQ_TYPE) {
	printf("Expected TraversalReq msg.\n");
        return -1;
      }
      xpos = lnic_read();
      ypos = lnic_read();
      lnic_read(); // discard timestamp
      // compute force on the particle
      force = compute_force(xcom, ycom, xpos, ypos);
      // send out TraversalResp
      lnic_write_r((app_hdr & (IP_MASK | CONTEXT_MASK)) | RESP_MSG_LEN);
      lnic_write_i(TRAVERSAL_RESP_TYPE);
      lnic_write_r(force);
      lnic_write_r(start_time);
      lnic_msg_done();
      msg_cnt++;
    }
  }
  return 0;
}

