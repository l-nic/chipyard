#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "mmio.h"
#include "icenic.h"

/**
 * Tasks:
 *   - receive a pkt
 *   - parse ethernet, ip, lnic headers
 *   - drop all non lnic pkts
 *   - swap eth src and dst
 *   - increment each word of the payload by 1
 *   - send pkt back out
 */
static int process_packet(void *buf)
{
  struct eth_header *eth;
  struct ipv4_header *ipv4;
  struct lnic_header *lnic;
  int len;
  uint64_t *data;
  int num_words;
  uint16_t msg_len;
  int i;

  // receive pkt
  len = nic_recv(buf);

  eth = buf;
  ipv4 = buf + ETH_HEADER_SIZE;
  // parse lnic hdr
  int ihl = ipv4->ver_ihl & 0xf;
  lnic = (void *)ipv4 + (ihl << 2);

  // swap eth src and dst
  swap_eth(eth);

  // increment every 8B word of the msg (except the last)
  data = (void *)lnic + LNIC_HEADER_SIZE;
  msg_len = ntohs(lnic->msg_len);
  num_words = msg_len/8;
  if (msg_len % 8 != 0) { num_words++; }
  for (i = 0; i < num_words-1; i++) {
    data[i] = htonl(ntohl(data[i]) + 1);
  }

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

