#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "mmio.h"
#include "nic.h"

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

  // receive pkt
  nic_recv(buf);

  // check eth hdr
  eth = buf;
  if (ntohs(eth->ethtype) != IPV4_ETHTYPE) {
    printf("Wrong ethtype %x\n", ntohs(eth->ethtype));
    return -1;
  }

  // check IPv4 hdr
  ipv4 = buf + sizeof(*eth);
  if (ipv4->proto != LNIC_PROTO) {
    printf("Wrong IP protocol %x\n", ipv4->proto);
    return -1;
  }

  // parse lnic hdr
  lnic = ipv4 + sizeof(*ipv4);

  // swap eth/ip/lnic src and dst
  memcpy(eth->dst_mac, eth->src_mac, MAC_ADDR_SIZE);
  memcpy(eth->src_mac, mac, MAC_ADDR_SIZE);

  tmp_ip_addr = ipv4->dst_addr;
  ipv4->dst_addr = ipv4->src_addr;
  ipv4->src_addr = tmp_ip_addr;

  tmp_lnic_addr = lnic->dst;
  lnic->dst = lnic->src;
  lnic->src = tmp_lnic_addr;

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

  printf("macaddr - %02x", macaddr[0]);
  for (int i = 1; i < MAC_ADDR_SIZE; i++)
    printf(":%02x", macaddr[i]);
  printf("\n");

  for (;;) {
    if (process_packet(buffer, macaddr))
      return -1;
  }

  return 0;
}

