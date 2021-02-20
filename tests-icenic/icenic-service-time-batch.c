#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "mmio.h"
#include "icenic.h"
#include "service-time.h"

#define NUM_BUFS 40
#define TSTAMP_BYTES 8

/**
 * Process each received request msg for the specified amount of time.
 */

struct service_time_header {
  uint64_t type;
};

struct config_header {
  uint64_t num_msgs;
  uint64_t timestamp;
};

struct data_req_header {
  uint64_t service_time;
};

uint64_t buffers[NUM_BUFS][ETH_MAX_WORDS];

int main(void)
{
  int msg_cnt;
  uint64_t num_msgs;
  uint64_t start_time;
  uint64_t service_time;
  int configured;
  int len;
  int i;

  // headers
  struct lnic_header *lnic;
  struct service_time_header *service;
  struct config_header *config;
  struct data_req_header *data;

  printf("Ready!\n");
  while(1) {
    msg_cnt = 0;
    configured = 0;
    // wait for a Config msg
    while(!configured) {
      nic_recv_lnic(buffers[0], &lnic);
      service = (void *)lnic + LNIC_HEADER_SIZE;
      if (ntohl(service->type) != CONFIG_TYPE) {
        printf("Expected Config msg.\n");
	return -1;
      }
      config = (void *)service + sizeof(struct service_time_header);
      num_msgs = ntohl(config->num_msgs);
      start_time = config->timestamp; // do not swap bytes
      configured = 1;
    }
    // process all messages
    while (msg_cnt < num_msgs) {
      // give the NIC all descriptors
      nic_post_recv_batch(buffers, NUM_BUFS);
      for (i = 0; i < NUM_BUFS; i++) {
        len = nic_wait_recv();
        lnic = (void *)buffers[i] + 34;
        service = (void *)lnic + LNIC_HEADER_SIZE;
        data = (void *)service + sizeof(struct service_time_header);
        service_time = ntohl(data->service_time);

        // process msg for specified amount of time
        process_msg(service_time);

        // send out response
        swap_eth(buffers[i]);
        // copy start_time into response pkt
        memcpy(buffers[i] + len - TSTAMP_BYTES, &start_time, TSTAMP_BYTES);
        nic_post_send(buffers[i], len);
        msg_cnt++;
      }
      // wait for all send operations to complete
      nic_wait_send_batch(NUM_BUFS);
    }
  }
  return 0;
}

