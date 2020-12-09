#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "mmio.h"
#include "icenic.h"
#include "encoding.h"

#define VECTOR_COUNT 10000
#define VECTOR_SIZE 64
#define VECTOR_SIZE_BYTES (VECTOR_SIZE * sizeof(uint16_t))
#define VECTOR_SIZE_WORDS (VECTOR_SIZE_BYTES / 8)

#include "euclidean-distance.c"

#define PRINT_TIMING 0
#define NCORES 1
#define USE_ONE_CONTEXT 1

uint16_t vectors[VECTOR_COUNT][VECTOR_SIZE];

uint32_t bswap16(uint32_t const x) {
    uint8_t data[2] = {};
    memcpy(&data, &x, sizeof(data));
    return ((uint16_t) data[1] << 0)
         | ((uint16_t) data[0] << 8);
}
uint32_t bswap32(uint32_t const x) {
    uint8_t data[4] = {};
    memcpy(&data, &x, sizeof(data));
    return ((uint32_t) data[3] << 0)
         | ((uint32_t) data[2] << 8)
         | ((uint32_t) data[1] << 16)
         | ((uint32_t) data[0] << 24);
}
uint64_t bswap64(uint64_t const x) {
    uint8_t data[8] = {};
    memcpy(&data, &x, sizeof(data));
    return ((uint64_t) data[7] << 0)
         | ((uint64_t) data[6] << 8)
         | ((uint64_t) data[5] << 16)
         | ((uint64_t) data[4] << 24)
         | ((uint64_t) data[3] << 32)
         | ((uint64_t) data[2] << 40)
         | ((uint64_t) data[1] << 48)
         | ((uint64_t) data[0] << 56);
}

void send_startup_msg(int cid, uint64_t context_id, uint64_t *buf, uint8_t *macaddr) {
  struct eth_header *eth = (struct eth_header *)buf;
  eth->ethtype = bswap16(IPV4_ETHTYPE);
  memcpy(eth->src_mac, macaddr, MAC_ADDR_SIZE);
  memcpy(eth->dst_mac, macaddr, MAC_ADDR_SIZE);
  eth->dst_mac[1] = 33;

  struct ipv4_header *ipv4 = (struct ipv4_header *)((char *)buf + ETH_HEADER_SIZE);
  memset(ipv4, 0, sizeof(*ipv4));
  ipv4->proto = LNIC_PROTO;
  ipv4->dst_addr = 0x0100000a;
  ipv4->src_addr = 0x00000000;
  ipv4->ver_ihl = 4<<4 | (0xf & 5);

  unsigned short msg_len = 2 * 8;
  ipv4->length = bswap16(sizeof(struct ipv4_header) + sizeof(struct lnic_header) + msg_len);

#define LNIC_DATA_FLAG_MASK        1
  struct lnic_header *lnic = (struct lnic_header *)((char *)ipv4 + sizeof(struct ipv4_header));
  memset(lnic, 0, sizeof(struct lnic_header));
  lnic->flags = LNIC_DATA_FLAG_MASK;
  lnic->msg_len = bswap16(msg_len);

  uint64_t *payload = (uint64_t *)((char *)lnic + sizeof(struct lnic_header));
  payload[0] = cid;
  payload[1] = context_id;

  size_t size = 66 + ETH_HEADER_SIZE;
  size = ceil_div(size, 8) * 8;
  nic_send(buf, size);
  //printf("send_startup_msg: sent %lu byte packet\n", size);
}

struct load_gen_header {
  uint64_t service_time;
  uint64_t sent_time;
} __attribute__((packed));
struct dist_req_header {
  uint16_t query_vector[VECTOR_SIZE];
  uint64_t haystack_vector_cnt;
  uint64_t haystack_vector_ids[12];
} __attribute__((packed));
struct dist_resp_header {
  uint64_t closest_vector_id;
} __attribute__((packed));

static int run_server(uint8_t *mac) {
  struct eth_header *eth;
  struct ipv4_header *ipv4;
  struct lnic_header *lnic;
  uint32_t tmp_ip_addr;
  uint16_t tmp_lnic_addr;
  ssize_t size;
  uint64_t buf[ETH_MAX_WORDS];
  uint16_t query_vector[VECTOR_SIZE];
#if PRINT_TIMING
  uint64_t t0, t1, t2, t3;
#endif // PRINT_TIMING

  printf("Server ready.\n");

  send_startup_msg(0, 0, buf, mac);

  while (1) {
    int recv_len = nic_recv(buf);
    (void)recv_len;

#if PRINT_TIMING
    t0 = rdcycle();
#endif // PRINT_TIMING

    // check eth hdr
    eth = (struct eth_header *)buf;
    if (bswap16(eth->ethtype) != IPV4_ETHTYPE) {
      printf("Wrong ethtype %x\n", bswap16(eth->ethtype));
      return -1;
    }

    // check IPv4 hdr
    ipv4 = (struct ipv4_header *)((char *)buf + ETH_HEADER_SIZE);
    if (ipv4->proto != LNIC_PROTO) {
      printf("Wrong IP protocol %x\n", ipv4->proto);
      return -1;
    }

    // parse lnic hdr
    int ihl = ipv4->ver_ihl & 0xf;
    lnic = (struct lnic_header *)((char *)ipv4 + (ihl << 2));

    // swap eth/ip/lnic src and dst
    memcpy(eth->dst_mac, eth->src_mac, MAC_ADDR_SIZE);
    memcpy(eth->src_mac, mac, MAC_ADDR_SIZE);

    tmp_ip_addr = ipv4->dst_addr;
    ipv4->dst_addr = ipv4->src_addr;
    ipv4->src_addr = tmp_ip_addr;

    tmp_lnic_addr = lnic->dst;
    lnic->dst = lnic->src;
    lnic->src = tmp_lnic_addr;

    struct dist_req_header *req = (struct dist_req_header *)(((char *)lnic) + sizeof(struct lnic_header) + sizeof(struct load_gen_header));
    for (unsigned i = 0; i < VECTOR_SIZE; i++)
      query_vector[i] = bswap16(req->query_vector[i]);
    uint64_t haystack_vector_cnt = bswap64(req->haystack_vector_cnt);
    // TODO: when running on firesim, does the IceNIC insert a timestamp in the
    // last word?
    uint64_t ingr_ts = *(uint64_t *)&req->haystack_vector_ids[haystack_vector_cnt];
    uint64_t closest_vector_id = 0, closest_vector_dist = 0;

#if PRINT_TIMING
    t1 = rdcycle();
#endif // PRINT_TIMING
    for (unsigned i = 0; i < haystack_vector_cnt; i++) {
      uint64_t vector_id = bswap64(req->haystack_vector_ids[i]);
      if (vector_id > VECTOR_COUNT) printf("Invalid vector_id=%lu\n", vector_id);
      uint64_t dist = compute_dist_squared(query_vector, vectors[vector_id-1]);
      if (closest_vector_id == 0 || dist < closest_vector_dist) {
        closest_vector_id = vector_id;
        closest_vector_dist = dist;
      }
    }
#if PRINT_TIMING
    t2 = rdcycle();
#endif // PRINT_TIMING

    struct dist_resp_header *resp = (struct dist_resp_header *)req;
    resp->closest_vector_id = bswap64(closest_vector_id);
    *(((uint64_t *)&resp->closest_vector_id)+1) = ingr_ts;

    uint64_t msg_len = 8 + 8 + 8;
    msg_len += 8; // append the timestamp
    ipv4->length = bswap16((ihl << 2) + sizeof(struct lnic_header) + msg_len);
    lnic->msg_len = bswap16(msg_len);

#if PRINT_TIMING
    t3 = rdcycle();
    printf("[%d] haystack_vectors=%lu  closest=%lu  Load lat: %ld    Compute lat: %ld    Send lat: %ld     Total lat: %ld\n", 0,
        haystack_vector_cnt, closest_vector_id, t1-t0, t2-t1, t3-t2, t3-t0);
#endif // PRINT_TIMING

    size = bswap16(ipv4->length) + ETH_HEADER_SIZE;
    size = ceil_div(size, 8) * 8;
    nic_send(buf, size);
  }

  return EXIT_SUCCESS;
}

int main(void) {
  uint64_t macaddr_long;
  uint8_t *macaddr;

  macaddr_long = nic_macaddr();
  macaddr = (uint8_t *) &macaddr_long;

#if 1
  for (unsigned i = 0; i < VECTOR_COUNT; i++)
    for (unsigned j = 0; j < VECTOR_SIZE; j+=8) {
      vectors[i][j+0] = 1; vectors[i][j+1] = 1; vectors[i][j+2] = 1; vectors[i][j+3] = 1;
      vectors[i][j+4] = 1; vectors[i][j+5] = 1; vectors[i][j+6] = 1; vectors[i][j+7] = 1;
    }
  //memset(vectors, 1, sizeof(vectors));
#endif

  return run_server(macaddr);
}
