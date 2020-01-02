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

#define ETH_MAX_WORDS 190
#define ETH_MAX_BYTES 1520
#define ETH_HEADER_SIZE 14
#define MAC_ADDR_SIZE 6
#define IP_ADDR_SIZE 4

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

static inline uint16_t htons(uint16_t nint)
{
        return ntohs(nint);
}

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


