#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "mmio.h"
#include "icenic.h"
#include "encoding.h"
#include "intersection.c"

#define MAX_QUERY_WORDS 8
#define MAX_INSERSECTION_DOCS 64

#define PRINT_TIMING 0

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

#if 0
uint32_t *word_to_docids[3];
uint32_t word_to_docids_bin[] = {3, // number of word records
    1, // word_id
    2, // number of doc_ids
    1, // doc_id 1
    2, // doc_id 2
    2, 3, 1, 3, 4,
    3, 3, 1, 4, 5};
#else
#include "word_to_docids.h"
#endif

unsigned word_cnt;

struct load_gen_header {
  uint64_t service_time;
  uint64_t sent_time;
} __attribute__((packed));
struct intersect_req_header {
  uint64_t query_word_cnt;
  uint64_t query_word_ids[MAX_QUERY_WORDS];
} __attribute__((packed));
struct intersect_resp_header {
  uint64_t doc_cnt;
  uint64_t doc_ids[MAX_INSERSECTION_DOCS];
} __attribute__((packed));

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

static int run_server(uint8_t *mac) {
  struct eth_header *eth;
  struct ipv4_header *ipv4;
  struct lnic_header *lnic;
  uint32_t tmp_ip_addr;
  uint16_t tmp_lnic_addr;
  ssize_t size;
  uint64_t buf[ETH_MAX_WORDS];
#if PRINT_TIMING
  uint64_t t0, t1, t2, t3;
#endif // PRINT_TIMING

  uint32_t query_word_ids[MAX_QUERY_WORDS];
  uint32_t *intersection_res, *intermediate_res;
  uint32_t intersection_tmp[2][1 + MAX_INSERSECTION_DOCS];


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

    struct intersect_req_header *req = (struct intersect_req_header *)(((char *)lnic) + sizeof(struct lnic_header) + sizeof(struct load_gen_header));

    uint64_t query_word_cnt = bswap64(req->query_word_cnt);
    // TODO: store two word_ids per 8-byte word in the packet
    for (unsigned i = 0; i < query_word_cnt; i++) {
      query_word_ids[i] = bswap64(req->query_word_ids[i]);
      if (query_word_ids[i] > word_cnt)
        printf("ERROR: received word_id > word_cnt (%u > %u)\n", query_word_ids[i], word_cnt);
    }

    // TODO: when running on firesim, does the IceNIC insert a timestamp in the
    // last word?
    uint64_t ingr_ts = req->query_word_ids[query_word_cnt];
#if PRINT_TIMING
    t1 = rdcycle();
#endif // PRINT_TIMING

    uint32_t word_id_ofst = query_word_ids[0]-1;
    intersection_res = word_to_docids[word_id_ofst];

    for (unsigned intersection_opr_cnt = 1; intersection_opr_cnt < query_word_cnt; intersection_opr_cnt++) {
      word_id_ofst = query_word_ids[intersection_opr_cnt]-1;
      intermediate_res = intersection_tmp[intersection_opr_cnt % 2];

      compute_intersection(intersection_res, word_to_docids[word_id_ofst], intermediate_res);
      intersection_res = intermediate_res;

      if (intersection_res[0] == 0) // stop if the intersection is empty
        break;
    }

#if PRINT_TIMING
    t2 = rdcycle();
#endif // PRINT_TIMING

    struct intersect_resp_header *resp = (struct intersect_resp_header *)req;
    unsigned intersection_size = intersection_res[0];
    resp->doc_cnt = bswap64(intersection_size);
    resp->doc_ids[intersection_size] = ingr_ts;
    for (unsigned i = 0; i < intersection_size; i++)
      resp->doc_ids[i] = bswap64(intersection_res[1+i]);

    uint64_t msg_len = 8 + 8 + 8 + (intersection_size * 8);
    msg_len += 8; // append the timestamp
    ipv4->length = bswap16((ihl << 2) + sizeof(struct lnic_header) + msg_len);
    lnic->msg_len = bswap16(msg_len);

#if PRINT_TIMING
    t3 = rdcycle();
    printf("[%d] query_words=%lu  res_docs=%u    Load lat: %ld    Compute lat: %ld    Send lat: %ld     Total lat: %ld\n", 0,
        query_word_cnt, intersection_size, t1-t0, t2-t1, t3-t2, t3-t0);
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

  load_docs(&word_cnt, word_to_docids, word_to_docids_bin);
  printf("Loaded %d words.\n", word_cnt);

  return run_server(macaddr);
}
