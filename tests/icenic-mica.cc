#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "mmio.h"
#include "icenic.h"
#include "encoding.h"

#define MICA_R_TYPE 1
#define MICA_W_TYPE 2

#define VALUE_SIZE_WORDS 64
#define VALUE_SIZE_BYTES (8 * VALUE_SIZE_WORDS)
#define KEY_SIZE_WORDS   2

#define USE_MICA 1
#define PRINT_TIMING 1

#if USE_MICA

#define NCORES 1
//#define MICA_SHM_BUFFER_SIZE (9233952) // 10K 512B items
//#define MICA_SHM_BUFFER_SIZE (8697016) // 10K 512B items
//#define MICA_SHM_BUFFER_SIZE (1133104) // 1K 512B items
//#define MICA_SHM_BUFFER_SIZE (566552) // 500 512B items
//#define MICA_SHM_BUFFER_SIZE (67648) // 100 512B items
#define MICA_SHM_BUFFER_SIZE (8456) // 10 512B items

#include "mica/table/fixedtable.h"

static constexpr size_t kValSize = VALUE_SIZE_WORDS * 8;

struct MyFixedTableConfig {
  static constexpr size_t kBucketCap = 15;

  // Support concurrent access. The actual concurrent access is enabled by
  // concurrent_read and concurrent_write in the configuration.
  static constexpr bool kConcurrent = false;

  // Be verbose.
  static constexpr bool kVerbose = false;

  // Collect fine-grained statistics accessible via print_stats() and
  // reset_stats().
  static constexpr bool kCollectStats = false;

  static constexpr size_t kKeySize = 8 * KEY_SIZE_WORDS;

  static constexpr bool concurrentRead = false;
  static constexpr bool concurrentWrite = false;

  //static constexpr size_t itemCount = 10000;
  static constexpr size_t itemCount = 10;
};

typedef mica::table::FixedTable<MyFixedTableConfig> FixedTable;
typedef mica::table::Result MicaResult;

static inline uint64_t rotate(uint64_t val, int shift) {
  // Avoid shifting by 64: doing so yields an undefined result.
  return shift == 0 ? val : ((val >> shift) | (val << (64 - shift)));
}
static inline uint64_t HashLen16(uint64_t u, uint64_t v, uint64_t mul) {
  // Murmur-inspired hashing.
  uint64_t a = (u ^ v) * mul;
  a ^= (a >> 47);
  uint64_t b = (v ^ a) * mul;
  b ^= (b >> 47);
  b *= mul;
  return b;
}
// This was extracted from the cityhash library. It's the codepath for hashing
// 16 byte values.
static inline uint64_t cityhash(const uint64_t *s) {
  static const uint64_t k2 = 0x9ae16a3b2f90404fULL;
  uint64_t mul = k2 + (KEY_SIZE_WORDS * 8) * 2;
  uint64_t a = s[0] + k2;
  uint64_t b = s[1];
  uint64_t c = rotate(b, 37) * mul + a;
  uint64_t d = (rotate(a, 25) + b) * mul;
  return HashLen16(c, d, mul);
}

#endif // USE_MICA


struct load_gen_header {
  uint64_t service_time;
  uint64_t sent_time;
} __attribute__((packed));
struct mica_req_header {
  uint64_t msg_type;
  uint64_t msg_key[2];
  uint64_t key_hash;
  uint64_t msg_value[VALUE_SIZE_WORDS];
  uint64_t timestamp;
} __attribute__((packed));
struct mica_read_resp_header {
  uint64_t msg_value[VALUE_SIZE_WORDS];
  uint64_t timestamp;
} __attribute__((packed));
struct mica_write_resp_header {
  uint64_t ack;
  uint64_t timestamp;
} __attribute__((packed));

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

#if USE_MICA
  uint64_t key_hash;
  MicaResult out_result;
  FixedTable::ft_key_t ft_key;
  FixedTable table(kValSize, 0);

  uint64_t init_value[VALUE_SIZE_WORDS];
  memset(init_value, 0, VALUE_SIZE_WORDS*8);

  printf("[%d] Inserting keys from %d to %ld.\n", 0, 1, MyFixedTableConfig::itemCount);
  for (unsigned i = 1; i < MyFixedTableConfig::itemCount; i++) {
    ft_key.qword[0] = i;
    ft_key.qword[1] = 0;
    init_value[0] = i;
    init_value[1] = i + 1;
    init_value[2] = i + 2;
    key_hash = cityhash(ft_key.qword);
    out_result = table.set(key_hash, ft_key, reinterpret_cast<char *>(init_value));
    if (out_result != MicaResult::kSuccess) printf("[%d] Inserting key %lu failed (%s).\n", 0, ft_key.qword[0], mica::table::cResultString(out_result));
    if (i % 100 == 0) printf("[%d] Inserted keys up to %d.\n", 0, i);
  }
#endif // USE_MICA

  printf("Server ready.\n");

  while (1) {

    // receive pkt
    int recv_len = nic_recv(buf);
    (void)recv_len;

#if PRINT_TIMING
    t0 = rdcycle();
#endif // PRINT_TIMING

    // check eth hdr
    eth = (struct eth_header *)buf;
    if (ntohs(eth->ethtype) != IPV4_ETHTYPE) {
      printf("Wrong ethtype %x\n", ntohs(eth->ethtype));
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

    struct mica_req_header *req = (struct mica_req_header *)(((char *)lnic) + sizeof(struct lnic_header) + sizeof(struct load_gen_header));

    uint64_t msg_type = ntohl(req->msg_type);
    ft_key.qword[0] = ntohl(req->msg_key[0]);
    ft_key.qword[1] = ntohl(req->msg_key[1]);

#if PRINT_TIMING
    t1 = rdcycle();
#endif // PRINT_TIMING
#if USE_MICA
    //key_hash = cityhash(ft_key.qword);
    key_hash = ntohl(req->key_hash);
#endif // USE_MICA
#if PRINT_TIMING
    t2 = rdcycle();
#endif // PRINT_TIMING

    uint64_t ingr_ts = req->timestamp;

    if (msg_type == MICA_R_TYPE) {
      struct mica_read_resp_header *read_resp = (struct mica_read_resp_header *)req;
#if USE_MICA
      out_result = table.get(key_hash, ft_key, (char *)read_resp->msg_value);
      if (out_result != MicaResult::kSuccess) {
        printf("[%d] GET failed for key %lu.\n", 0, ft_key.qword[0]);
      }
#endif // USE_MICA
      read_resp->timestamp = ingr_ts;
      ipv4->length = htons((ihl << 2) + sizeof(struct lnic_header) + sizeof(struct load_gen_header) + sizeof(struct mica_read_resp_header));
      lnic->msg_len = htons(sizeof(struct load_gen_header) + sizeof(struct mica_read_resp_header));
    }
    else {
      struct mica_write_resp_header *write_resp = (struct mica_write_resp_header *)req;
#if USE_MICA
      out_result = table.set(key_hash, ft_key, (const char *)req->msg_value);
      if (out_result != MicaResult::kSuccess) {
        printf("[%d] Inserting key %lu failed.\n", 0, ft_key.qword[0]);
      }
#endif // USE_MICA
      write_resp->ack = 0x1; // ACK the write
      write_resp->timestamp = ingr_ts;
      ipv4->length = htons((ihl << 2) + sizeof(struct lnic_header) + sizeof(struct load_gen_header) + sizeof(struct mica_write_resp_header));
      lnic->msg_len = htons(sizeof(struct load_gen_header) + sizeof(struct mica_write_resp_header));
    }

#if PRINT_TIMING
    t3 = rdcycle();
    printf("[%d] %s key=%lu. Hash lat: %ld     MICA latency: %ld     Total latency: %ld\n", 0,
        msg_type == MICA_R_TYPE ? "GET" : "SET", ft_key.qword[0], t2-t1, t3-t2, t3-t0);
#endif // PRINT_TIMING

    // send pkt back out
    size = ntohs(ipv4->length) + ETH_HEADER_SIZE;
    size = ceil_div(size, 8) * 8;
    nic_send(buf, size);
  }

  return EXIT_SUCCESS;
}

int main(void)
{
  uint64_t macaddr_long;
  uint8_t *macaddr;

  macaddr_long = nic_macaddr();
  macaddr = (uint8_t *) &macaddr_long;

  return run_server(macaddr);
}

