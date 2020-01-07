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
 *   - swap eth/ip/lnic src and dst
 *   - send pkt back out
 */
static int process_packet(void *buf, uint8_t *mac)
{
  struct eth_header *eth;
  struct ipv4_header *ipv4;
  struct lnic_header *lnic;
  uint32_t tmp_ip_addr;
  uint16_t tmp_lnic_addr;
  ssize_t size;
  uint64_t *data;
  int num_words;
  uint16_t msg_len;
  int i;

  // receive pkt
  nic_recv(buf);

  // check eth hdr
  eth = buf;
  if (ntohs(eth->ethtype) != IPV4_ETHTYPE) {
    printf("Wrong ethtype %x\n", ntohs(eth->ethtype));
    return -1;
  }

  // check IPv4 hdr
  ipv4 = buf + ETH_HEADER_SIZE;
  if (ipv4->proto != LNIC_PROTO) {
    printf("Wrong IP protocol %x\n", ipv4->proto);
    return -1;
  }

  // parse lnic hdr
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

  // increment every 8B word of the msg (except the last)
  data = (void *)lnic + LNIC_HEADER_SIZE;
  msg_len = ntohs(lnic->msg_len);
  num_words = msg_len/8;
  if (msg_len % 8 != 0) { num_words++; }
  for (i = 0; i < num_words-1; i++) {
    data[i] = htonl(ntohl(data[i]) + 1);
  }

  // send pkt back out
  size = ntohs(ipv4->length) + ETH_HEADER_SIZE;
  size = ceil_div(size, 8) * 8;
  nic_send(buf, size);

  return 0;
}

uint64_t buffer[ETH_MAX_WORDS];

int main(void)
{
  uint64_t macaddr_long;
  uint8_t *macaddr;

  macaddr_long = nic_macaddr();
  macaddr = (uint8_t *) &macaddr_long;

  for (;;) {
    if (process_packet(buffer, macaddr))
      return -1;
  }

  return 0;
}

