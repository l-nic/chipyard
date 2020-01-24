#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "lnic.h"

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

#define CONFIG_TYPE 0
#define TRAVERSAL_REQ_TYPE 1
#define TRAVERSAL_RESP_TYPE 2

#define THETA 0.5
#define RESP_MSG_LEN 24

// TODO(sibanez): implement this ...
int compute_force(double xcom, double ycom, double xpos, double ypos, double *force, int *valid) {
  // compute force on the particle
  // assume unit masses and theta = 0.5
  // If the particle is sufficiently far away then set valid and compute force.
  // Otherwise, unset valid.
  *force = ((xcom - xpos) + (ycom - ypos)) / 2.0;
  *valid = 1;
}

int main(void) {
  // register context ID with L-NIC
  lnic_add_context(0, 0);

  // local variables
  uint64_t app_hdr;
  uint64_t msg_type;
  double xcom, ycom;
  double xpos, ypos;
  int msg_cnt;
  uint64_t num_msgs;
  double force;
  int valid;
  uint64_t start_time;
  int configured;

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
      xcom = (double)lnic_read();
      ycom = (double)lnic_read();
//      printf("xcom = %lx\n", (uint64_t)xcom);
//      printf("ycom = %lx\n", (uint64_t)ycom);
      num_msgs = lnic_read();
      start_time = lnic_read();
      configured = 1;
    }
    // process all requests and send one response at the end
    while (msg_cnt < num_msgs) {
      lnic_wait();
      app_hdr = lnic_read(); // read app hdr
      if (lnic_read() != TRAVERSAL_REQ_TYPE) {
	printf("Expected TraversalReq msg.\n");
        return -1;
      }
      xpos = (double)lnic_read();
      ypos = (double)lnic_read();
//      printf("xpos = %lx\n", (uint64_t)xpos);
//      printf("ypos = %lx\n", (uint64_t)ypos);
      lnic_read(); // discard timestamp
      // compute force on the particle
      compute_force(xcom, ycom, xpos, ypos, &force, &valid);
//      printf("force = %lx\n", (uint64_t)force);
      // TODO(sibanez): should really send either TraversalResp or TraversalReq for each msg that is processed, but we won't do that yet
      msg_cnt++;
    }
    // send out TraversalResp
    lnic_write_r((app_hdr & (IP_MASK | CONTEXT_MASK)) | RESP_MSG_LEN);
    lnic_write_i(TRAVERSAL_RESP_TYPE);
    lnic_write_r((uint64_t)force);
    lnic_write_r(start_time);
  }
  return 0;
}

