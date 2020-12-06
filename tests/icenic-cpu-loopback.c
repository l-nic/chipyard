#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "encoding.h"
#include "mmio.h"
#include "icenic.h"

/**
 * Tasks:
 *   - receive a pkt
 *   - parse ethernet headers
 *   - swap eth src and dst
 *   - send pkt back out
 */
static int process_packet(void *buf)
{
  struct eth_header *eth;
  int len;

  // receive pkt
  len = nic_recv(buf);
  eth = buf;
  // swap eth src and dst
  uint8_t tmp_mac[MAC_ADDR_SIZE];
  memcpy(tmp_mac, eth->dst_mac, MAC_ADDR_SIZE);
  memcpy(eth->dst_mac, eth->src_mac, MAC_ADDR_SIZE);
  memcpy(eth->src_mac, tmp_mac, MAC_ADDR_SIZE);
  // send pkt back out
  nic_send(buf, len);

  return 0;
}

uint64_t buffer[ETH_MAX_WORDS];

int main(void)
{
  printf("Ready!\n");
  for (;;) {
    if (process_packet(buffer))
      return -1;
  }

  return 0;
}

