#define SIMPLENIC_BASE 0x10016000L
#define SIMPLENIC_SEND_REQ (SIMPLENIC_BASE + 0)
#define SIMPLENIC_RECV_REQ (SIMPLENIC_BASE + 8)
#define SIMPLENIC_SEND_COMP (SIMPLENIC_BASE + 16)
#define SIMPLENIC_RECV_COMP (SIMPLENIC_BASE + 18)
#define SIMPLENIC_COUNTS (SIMPLENIC_BASE + 20)
#define SIMPLENIC_MACADDR (SIMPLENIC_BASE + 24)

static inline int nic_send_req_avail(void)
{
	return reg_read32(SIMPLENIC_COUNTS) & 0xff;
}

static inline int nic_recv_req_avail(void)
{
	return (reg_read32(SIMPLENIC_COUNTS) >> 8) & 0xff;
}

static inline int nic_send_comp_avail(void)
{
	return (reg_read32(SIMPLENIC_COUNTS) >> 16) & 0xff;
}

static inline int nic_recv_comp_avail(void)
{
	return (reg_read32(SIMPLENIC_COUNTS) >> 24) & 0xff;
}

static void nic_send(void *data, unsigned long len)
{
	uintptr_t addr = ((uintptr_t) data) & ((1L << 48) - 1);
	unsigned long packet = (len << 48) | addr;

	while (nic_send_req_avail() == 0);
	reg_write64(SIMPLENIC_SEND_REQ, packet);

	while (nic_send_comp_avail() == 0);
	reg_read16(SIMPLENIC_SEND_COMP);
}

static int nic_recv(void *dest)
{
	uintptr_t addr = (uintptr_t) dest;
	int len;

	while (nic_recv_req_avail() == 0);
	reg_write64(SIMPLENIC_RECV_REQ, addr);

	// Poll for completion
	while (nic_recv_comp_avail() == 0);
	len = reg_read16(SIMPLENIC_RECV_COMP);
	asm volatile ("fence");

	return len;
}

static inline uint64_t nic_macaddr(void)
{
	return reg_read64(SIMPLENIC_MACADDR);
}

#define IPV4_ETHTYPE 0x0800
#define ARP_ETHTYPE 0x0806
#define ICMP_PROTO 1
#define LNIC_PROTO 0x99
#define ECHO_REPLY 0
#define ECHO_REQUEST 8
#define ARP_REQUEST 1
#define ARP_REPLY 2
#define HTYPE_ETH 1

#define ceil_div(n, d) (((n) - 1) / (d) + 1)

static inline uint16_t ntohs(uint16_t nint)
{
        return ((nint & 0xff) << 8) | ((nint >> 8) & 0xff);
}

static inline uint32_t ntohi(uint32_t nint)
{
        return (((uint32_t)ntohs(nint & 0xffff) << 16) | (ntohs(nint >> 16) & 0xffff));
}

static inline uint64_t ntohl(uint64_t nint)
{
	return (((uint64_t)ntohi(nint & 0xffffffff) << 32) | (ntohi(nint >> 32) & 0xffffffff));
}

static inline uint16_t htons(uint16_t nint)
{
        return ntohs(nint);
}

static inline uint32_t htoni(uint32_t nint)
{
	return ntohi(nint);
}

static inline uint64_t htonl(uint64_t nint)
{
	return ntohl(nint);
}

#define ETH_MAX_WORDS 190
#define ETH_MAX_BYTES 1520
#define ETH_HEADER_SIZE 14
#define MAC_ADDR_SIZE 6
#define IP_ADDR_SIZE 4

#define LNIC_HEADER_SIZE 14

struct eth_header {
        uint8_t dst_mac[MAC_ADDR_SIZE];
        uint8_t src_mac[MAC_ADDR_SIZE];
        uint16_t ethtype;
};

struct arp_header {
        uint16_t htype;
        uint16_t ptype;
        uint8_t hlen;
        uint8_t plen;
        uint16_t oper;
        uint8_t sha[MAC_ADDR_SIZE];
        uint8_t spa[IP_ADDR_SIZE];
        uint8_t tha[MAC_ADDR_SIZE];
        uint8_t tpa[IP_ADDR_SIZE];
};

struct ipv4_header {
        uint8_t ver_ihl;
        uint8_t dscp_ecn;
        uint16_t length;
        uint16_t ident;
        uint16_t flags_frag_off;
        uint8_t ttl;
        uint8_t proto;
        uint16_t cksum;
        uint32_t src_addr;
        uint32_t dst_addr;
};

struct icmp_header {
        uint8_t type;
        uint8_t code;
        uint16_t cksum;
        uint32_t rest;
};

struct lnic_header {
	uint16_t src;
	uint16_t dst;
	uint16_t msg_id;
	uint16_t msg_len;
	uint16_t offset;
	uint32_t padding;
};

static int checksum(uint16_t *data, int len)
{
        int i;
        uint32_t sum = 0;

        for (i = 0; i < len; i++)
                sum += ntohs(data[i]);

        while ((sum >> 16) != 0)
                sum = (sum & 0xffff) + (sum >> 16);

        sum = ~sum & 0xffff;

        return sum;
}

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
