#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "mmio.h"
#include "nic.h"

#define ceil_div(n, d) (((n) - 1) / (d) + 1)

static int process_arp(void *buf, uint8_t *mac)
{
	struct eth_header *eth = buf;
	struct arp_header *arp;
	size_t size = ETH_HEADER_SIZE + sizeof(*arp);
	uint8_t tmp_addr[IP_ADDR_SIZE];

	// Verify arp packet
	arp = buf + sizeof(*eth);
	if (ntohs(arp->oper) != ARP_REQUEST) {
		printf("Wrong arp operation: %d\n", ntohs(arp->oper));
		return -1;
	}

	if (ntohs(arp->htype) != HTYPE_ETH) {
		printf("Wrong ARP HTYPE\n");
		return -1;
	}

	if (ntohs(arp->ptype) != IPV4_ETHTYPE) {
		printf("Wrong ARP PTYPE\n");
		return -1;
	}

	if (arp->hlen != 6) {
		printf("Wrong ARP HLEN: %d\n", arp->hlen);
		return -1;
	}

	if (arp->plen != 4) {
		printf("Wrong ARP PLEN: %d\n", arp->plen);
		return -1;
	}

	// Make the source the destination, and add our mac address
	memcpy(eth->dst_mac, eth->src_mac, MAC_ADDR_SIZE);
	memcpy(eth->src_mac, mac, MAC_ADDR_SIZE);

	// create ARP reply
	arp->oper = htons(ARP_REPLY);

	// Make tha the sha, and fill in sha with actual mac address
	memcpy(arp->tha, arp->sha, MAC_ADDR_SIZE);
	memcpy(arp->sha, mac, MAC_ADDR_SIZE);

	// Swap spa and tpa in arp packet
	memcpy(tmp_addr, arp->tpa, IP_ADDR_SIZE);
	memcpy(arp->tpa, arp->spa, IP_ADDR_SIZE);
	memcpy(arp->spa, tmp_addr, IP_ADDR_SIZE);

	size = ceil_div(size, 8) * 8;
	nic_send(buf, size);

	return 0;
}
static int process_icmp(void *buf, uint8_t *mac)
{
	struct eth_header *eth = buf;
	struct ipv4_header *ipv4;
	struct icmp_header *icmp;
	int ihl, icmp_size;
	ssize_t size;
	uint32_t tmp_addr;

	// verify IPv4
	ipv4 = buf + sizeof(*eth);
	ihl = ipv4->ver_ihl & 0xf;

	if (checksum((uint16_t *) ipv4, ihl << 1) != 0) {
		printf("Bad IP header checksum %04x\n", ipv4->cksum);
		return -1;
	}

	if (ipv4->proto != ICMP_PROTO) {
		printf("Wrong IP protocol %d\n", ipv4->proto);
		return -1;
	}

	// verify ICMP
	icmp = (buf + sizeof(*eth) + (ihl << 2));

	if (icmp->type != ECHO_REQUEST) {
		printf("Wrong ICMP type %d\n", icmp->type);
		return -1;
	}

	if (icmp->code != 0) {
		printf("Wrong ICMP code %d\n", icmp->code);
		return -1;
	}

	icmp_size = ntohs(ipv4->length) - (ihl << 2);
	if (checksum((uint16_t *) icmp, icmp_size >> 1) != 0) {
		printf("Bad ICMP checksum %04x\n", icmp->cksum);
		return -1;
	}

	// Set the destination and source MACs
	memcpy(eth->dst_mac, eth->src_mac, MAC_ADDR_SIZE);
	memcpy(eth->src_mac, mac, MAC_ADDR_SIZE);

	// Swap the source and destination IP addresses
	tmp_addr = ipv4->dst_addr;
	ipv4->dst_addr = ipv4->src_addr;
	ipv4->src_addr = tmp_addr;

	// compute the IPv4 header checksum
	ipv4->cksum = 0;
	ipv4->cksum = htons(checksum((uint16_t *) ipv4, ihl << 1));

	// set the ICMP type to reply and compute checksum
	icmp->cksum = 0;
	icmp->type = ECHO_REPLY;
	icmp->cksum = htons(checksum((uint16_t *) icmp, icmp_size >> 1));
	size = ntohs(ipv4->length) + ETH_HEADER_SIZE;

	size = ceil_div(size, 8) * 8;
	nic_send(buf, size);

	return 0;
}

static int process_packet(void *buf, uint8_t *mac)
{
	struct eth_header *eth;

	// read the ICMP request
	nic_recv(buf);
	eth = buf;
	printf("Got packet: [ethtype=%04x]\n", ntohs(eth->ethtype));
	// Check ethernet type
	switch (ntohs(eth->ethtype)) {
	case IPV4_ETHTYPE:
		return process_icmp(buf, mac);
	case ARP_ETHTYPE:
		return process_arp(buf, mac);
	default:
		printf("Wrong ethtype %x\n", ntohs(eth->ethtype));
		return -1;
	}
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
