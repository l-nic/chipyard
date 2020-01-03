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

#define DATA_PKT_LEN 72

#define NN_HEADER_SIZE 8
struct nn_header {
  uint64_t type;
};

#define CONFIG_HEADER_SIZE 8
struct config_header {
  uint64_t num_edges;
};

#define WEIGHT_HEADER_SIZE 16
struct weight_header {
  uint64_t index;
  uint64_t weight;
};

#define DATA_HEADER_SIZE 16
struct data_header {
  uint64_t index;
  uint64_t data;
};

/**
 * Receive and parse Eth/IP/LNIC headers.
 * Only return once lnic pkt is received.
 */
static int nic_recv_lnic(void *buf, struct lnic_header **lnic)
{
  struct eth_header *eth;
  struct ipv4_header *ipv4;

  while (1) {
    // receive pkt
    nic_recv(buf);

    // check eth hdr
    eth = buf;
    if (ntohs(eth->ethtype) != IPV4_ETHTYPE) {
      printf("Wrong ethtype %x\n", ntohs(eth->ethtype));
      break;
    }

    // check IPv4 hdr
    ipv4 = buf + ETH_HEADER_SIZE;
    if (ipv4->proto != LNIC_PROTO) {
      printf("Wrong IP protocol %x\n", ipv4->proto);
      break;
    }

    // parse lnic hdr
    int ihl = ipv4->ver_ihl & 0xf;
    *lnic = (void *)ipv4 + (ihl << 2);
    return 0;
  }
  return 0;
}

/**
 * Swap addresses in lnic pkt
 */
static int swap_addresses(void *buf, uint8_t *mac)
{
  struct eth_header *eth;
  struct ipv4_header *ipv4;
  struct lnic_header *lnic;
  uint32_t tmp_ip_addr;
  uint16_t tmp_lnic_addr;

  eth = buf;
  ipv4 = buf + ETH_HEADER_SIZE;
  int ihl = ipv4->ver_ihl & 0xf;
  lnic = (void *)ipv4 + (ihl << 2);

  // swap eth/ip/lnic src and dst
  memcpy(eth->dst_mac, eth->src_mac, MAC_ADDR_SIZE);
  memcpy(eth->src_mac, mac, MAC_ADDR_SIZE);

  tmp_ip_addr = ipv4->dst_addr;
  ipv4->dst_addr = ipv4->src_addr;
  ipv4->src_addr = tmp_ip_addr;

  tmp_lnic_addr = lnic->dst;
  lnic->dst = lnic->src;
  lnic->src = tmp_lnic_addr;

  return 0;
}

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
    size = ceil_div(DATA_PKT_LEN, 8) * 8;
    nic_send(buffer, size);
  }
  return 0;
}

