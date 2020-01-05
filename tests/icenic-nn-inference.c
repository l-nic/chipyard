#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "mmio.h"
#include "icenic.h"

/**
 * Simple NN inference application on IceNIC.
 */

#define MAX_NUM_EDGES 128

#define CONFIG_TYPE 0
#define WEIGHT_TYPE 1
#define DATA_TYPE 2

#define NN_HEADER_SIZE 8
struct nn_header {
  uint64_t type;
};

#define CONFIG_HEADER_SIZE 16
struct config_header {
  uint64_t num_edges;
  uint64_t timestamp;
};

#define WEIGHT_HEADER_SIZE 24
struct weight_header {
  uint64_t index;
  uint64_t weight;
  uint64_t timestamp;
};

#define DATA_HEADER_SIZE 24
struct data_header {
  uint64_t index;
  uint64_t data;
  uint64_t timestamp;
};

#define DATA_PKT_LEN (ETH_HEADER_SIZE + 20 + LNIC_HEADER_SIZE + NN_HEADER_SIZE + DATA_HEADER_SIZE)

uint64_t buffer[ETH_MAX_WORDS];

int main(void)
{
  // allocate stack memory for weights
  uint64_t weights[MAX_NUM_EDGES];
  int edge_cnt;
  uint64_t num_edges;
  uint64_t result;
  char configured;
  ssize_t size;
  uint64_t start_time;

  // headers
  struct lnic_header *lnic;
  struct nn_header *nn;
  struct config_header *config;
  struct weight_header *weight;
  struct data_header *data;

  uint64_t macaddr_long;
  uint8_t *macaddr;

  macaddr_long = nic_macaddr();
  macaddr = (uint8_t *) &macaddr_long;

  while(1) {
    edge_cnt = 0;
    result = 0;
    // wait for Config msg
    configured = 0;
    while (!configured) {
      // receive lnic pkt
      nic_recv_lnic(buffer, &lnic);
      nn = (void *)lnic + LNIC_HEADER_SIZE;
      if (ntohl(nn->type) == CONFIG_TYPE) {
	config = (void *)nn + NN_HEADER_SIZE;
        num_edges = ntohl(config->num_edges);
        configured = 1;
	start_time = ntohl(config->timestamp);
      }
    }

    // process weight and data msgs
    while (edge_cnt < num_edges) {
      // receive lnic pkt
      nic_recv_lnic(buffer, &lnic);
      nn = (void *)lnic + LNIC_HEADER_SIZE;
      if (ntohl(nn->type) == WEIGHT_TYPE) {
	weight = (void *)nn + NN_HEADER_SIZE;
        weights[ntohl(weight->index)] = ntohl(weight->weight);
      } else if (ntohl(nn->type) == DATA_TYPE) {
	data = (void *)nn + NN_HEADER_SIZE;
        result += weights[ntohl(data->index)] * ntohl(data->data);
        edge_cnt++;
      }
    }

    // send out result data pkt
    swap_addresses(buffer, macaddr);
    nn->type = htonl(DATA_TYPE);
    data = (void *)nn + NN_HEADER_SIZE;
    data->index = 0;
    data->data = htonl(result);
    data->timestamp = htonl(start_time);
    size = ceil_div(DATA_PKT_LEN, 8) * 8;
    nic_send(buffer, size);
  }
  return 0;
}

