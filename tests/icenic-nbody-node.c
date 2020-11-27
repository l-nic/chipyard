#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "mmio.h"
#include "icenic.h"
#include "nbody.h"

/**
 * N-Body Node simulation application on IceNIC.
 */

#define NBODY_HEADER_SIZE 8
struct nbody_header {
  uint64_t type;
};

#define CONFIG_HEADER_SIZE 8*4
struct config_header {
  uint64_t xcom;
  uint64_t ycom;
  uint64_t num_msgs;
  uint64_t timestamp;
};

#define TRAVERSAL_REQ_HEADER_SIZE 8*3
struct traversal_req_header {
  uint64_t xpos;
  uint64_t ypos;
  uint64_t timestamp;
};

#define TRAVERSAL_RESP_HEADER_SIZE 8*2
struct traversal_resp_header {
  uint64_t force;
  uint64_t timestamp;
};

#define RESP_PKT_LEN (ETH_HEADER_SIZE + 20 + LNIC_HEADER_SIZE + NBODY_HEADER_SIZE + TRAVERSAL_RESP_HEADER_SIZE)

uint64_t buffer[ETH_MAX_WORDS];

int main(void)
{
  // local variables
  uint64_t app_hdr;
  uint64_t msg_type;
  uint64_t xcom, ycom;
  uint64_t xpos, ypos;
  int msg_cnt;
  uint64_t num_msgs;
  uint64_t force;
  uint64_t start_time;
  int configured;
  ssize_t size;

  // headers
  struct lnic_header *lnic;
  struct nbody_header *nbody;
  struct config_header *config;
  struct traversal_req_header *traversal_req;
  struct traversal_resp_header *traversal_resp;

  printf("Ready!\n");
  while(1) {
    msg_cnt = 0;
    configured = 0;
    // wait for a Config msg
    while(!configured) {
      nic_recv_lnic(buffer, &lnic);
      nbody = (void *)lnic + LNIC_HEADER_SIZE;
      if (ntohl(nbody->type) != CONFIG_TYPE) {
        printf("Expected Config msg.\n");
	return -1;
      }
      config = (void *)nbody + NBODY_HEADER_SIZE;
      xcom = ntohl(config->xcom);
      ycom = ntohl(config->ycom);
      num_msgs = ntohl(config->num_msgs);
      start_time = ntohl(config->timestamp);
      configured = 1;
    }
    // process all requests and send one response at the end
    while (msg_cnt < num_msgs) {
      nic_recv_lnic(buffer, &lnic);
      nbody = (void *)lnic + LNIC_HEADER_SIZE;
      if (ntohl(nbody->type) != TRAVERSAL_REQ_TYPE) {
	printf("Expected TraversalReq msg.\n");
        return -1;
      }
      traversal_req = (void *)nbody + NBODY_HEADER_SIZE;
      xpos = ntohl(traversal_req->xpos);
      ypos = ntohl(traversal_req->ypos);
      force = compute_force(xcom, ycom, xpos, ypos);
      // send out TraversalResp
      swap_eth(buffer);
      nbody->type = htonl(TRAVERSAL_RESP_TYPE);
      traversal_resp = (void *)nbody + NBODY_HEADER_SIZE;
      traversal_resp->force = htonl(force);
      traversal_resp->timestamp = htonl(start_time);
      size = ceil_div(RESP_PKT_LEN, 8) * 8;
      nic_send(buffer, size);
      msg_cnt++;
    }
  }
  return 0;
}

