#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <string.h>
#include <stdbool.h>
#ifndef __riscv
#include <algorithm>
#endif
#include "lnic.h"

#define NCORES 4
#define NNODES 4

#define GLOBAL_M                (NCORES * NNODES)
#define KEYS_PER_NODE           32
#define NODES_PER_PIVOT_SORTER  4
#define NUM_PIVOT_SORTERS       (GLOBAL_M / NODES_PER_PIVOT_SORTER)
#define MAX_KEYS_PER_NODE       (KEYS_PER_NODE * 3)
#define MAX_VALS_PER_PKT        100
#define VALUE_SIZE_WORDS        12

#define NUM_L1_PIVOTS               GLOBAL_M
#define NUM_L2_PIVOTS               NUM_PIVOT_SORTERS
#define NUM_L2_SPLITTERS            NUM_PIVOT_SORTERS
#define NUM_L1_SPLITTERS            GLOBAL_M
#define MAX_L1_SPLITTERS_PER_NODE   ((NUM_L1_SPLITTERS / NUM_PIVOT_SORTERS) * 3)


#define MSG_TYPE_L1_PIVOTS    1
#define MSG_TYPE_L2_PIVOTS    2
#define MSG_TYPE_L2_SPLITTERS 3
#define MSG_TYPE_SHUF_PIVOTS  4
#define MSG_TYPE_L1_SPLITTERS 5
#define MSG_TYPE_ITEM         6

#define MAX_MSG_TYPE 6

#define ITEM_MSG_SIZE_WORDS (2 + 1 + 1 + VALUE_SIZE_WORDS) // {msg_type, key, value}
#define L1_PIVOTS_MSG_SIZE_WORDS (2 + 1 + NUM_L1_PIVOTS) // {msg_type, keys}
#define L2_PIVOTS_MSG_SIZE_WORDS (2 + 1 + NUM_L2_PIVOTS) // {msg_type, keys}
#define L2_SPLITTERS_MSG_SIZE_WORDS (2 + 1 + NUM_L2_SPLITTERS) // {msg_type, keys}

#define FLAG_NULL           (1 << 8)
#define FLAG_LAST           (1 << 9)
#define GET_RANK(md)        ((md >> 16) & 0xffff)
#define SET_RANK(r)         ((r & 0xffff) << 16)

#define PIV_SORTER_IDX(nid) (nid / NUM_PIVOT_SORTERS)

#define BASE_NODE_IP 0x0a000002
#define SWITCH_IP 0x0a000001


#ifdef __riscv
#define myassert(x) if (!(x)) printf("ASSERTION FAILED: %s:%d %s()\n", __FILE__, __LINE__,  __func__)
#else
#include <assert.h>
#define myassert assert
#endif

#ifndef __riscv
#include <vector>
const std::vector<uint32_t> g_node_ips = {
  BASE_NODE_IP+0, BASE_NODE_IP+1, BASE_NODE_IP+2, BASE_NODE_IP+3,
  BASE_NODE_IP+4, BASE_NODE_IP+5, BASE_NODE_IP+6, BASE_NODE_IP+7,
  BASE_NODE_IP+8, BASE_NODE_IP+9, BASE_NODE_IP+10, BASE_NODE_IP+11,
  BASE_NODE_IP+12, BASE_NODE_IP+13, BASE_NODE_IP+14, BASE_NODE_IP+15,
};
const int g_nc = NCORES;
#endif

#ifdef __cplusplus
extern "C" {
#endif

static unsigned g_seed = 0;
static inline int fastrand() { // Source: https://stackoverflow.com/a/3747462
  g_seed = (214013*g_seed+2531011);
  return g_seed >> 1;
}
#define rand fastrand

volatile bool g_did_init = false;
uint64_t g_node_addrs[GLOBAL_M];

#define USE_MYHEAP 1
#ifdef __riscv
#define MYHEAP_SIZE (64 * 1024)
#else
#define MYHEAP_SIZE (64 * 1024 * NNODES)
#endif
#if USE_MYHEAP
char g_myheap[MYHEAP_SIZE];
static int g_myheap_head = 0;
#endif

static unsigned g_total_alloc = 0;
void *mymalloc(size_t s) {
  g_total_alloc += s;
#if USE_MYHEAP
  void *p = s == 0 ? NULL : &g_myheap[g_myheap_head];
  g_myheap_head += s;
  g_myheap_head += (8 - (s % 8)) % 8; // align to 8-byte word
  myassert(g_myheap_head < MYHEAP_SIZE);
#else
  void *p = malloc(s);
#endif
  if (g_did_init) printf("malloc(%ld): %p\n", s, p);
  return p;
}
void *mycalloc(size_t a, size_t b) {
#if USE_MYHEAP
  void *p = mymalloc(a*b);
#else
  g_total_alloc += a*b;
  void *p = calloc(a, b);
#endif
  if (g_did_init) printf("calloc(%ld): %p\n", a*b, p);
  return p;
}
void myfree(void *p) {
  //if (g_did_init) printf("free(%p)\n", p);
#if USE_MYHEAP
#else
  return free(p);
#endif
}
void *myrealloc(void *p, size_t sz) {
#if USE_MYHEAP
  if (sz == 0) { myfree(p); return NULL; }
  char *p2 = (char *)mymalloc(sz);
  if (p != NULL) memcpy(p2, p, sz);
#else
  g_total_alloc += sz;
  void *p2 = realloc(p, sz);
#endif
  if (g_did_init) printf("realloc(%p, %ld): %p\n", p, sz, p2);
  return p2;
}

#define kmalloc mymalloc
#define kcalloc mycalloc
#define kfree myfree
#define krealloc myrealloc

#include "khash.h"
KHASH_MAP_INIT_INT64(m64, unsigned)

#include "qsort.h"
void isort(uint64_t A[], size_t n) {
  uint64_t tmp;
#define LESS(i, j) A[i] < A[j]
#define SWAP(i, j) tmp = A[i], A[i] = A[j], A[j] = tmp
  QSORT(n, LESS, SWAP);
}

#define MAX(a,b) __extension__({ __typeof__ (a) _a = (a);  __typeof__ (b) _b = (b); _a > _b ? _a : _b; })
#define MIN(a,b) __extension__({ __typeof__ (a) _a = (a);  __typeof__ (b) _b = (b); _a < _b ? _a : _b; })

void printArr(uint64_t *a, int n) {
  for (int i = 0; i < n; i++) { printf(" %ld", a[i]); } printf("\n");
}
#ifdef __riscv
typedef struct {
  volatile unsigned int lock;
} arch_spinlock_t;

#define arch_spin_is_locked(x) ((x)->lock != 0)
static inline void arch_spin_unlock(arch_spinlock_t *lock) {
  asm volatile (
    "amoswap.w.rl x0, x0, %0"
    : "=A" (lock->lock)
    :: "memory"
    );
}
static inline int arch_spin_trylock(arch_spinlock_t* lock) {
  int tmp = 1, busy;
  asm volatile (
    "amoswap.w.aq %0, %2, %1"
    : "=r"(busy), "+A" (lock->lock)
    : "r"(tmp)
    : "memory"
    );
  return !busy;
}
static inline void arch_spin_lock(arch_spinlock_t* lock) {
  while (1) {
    if (arch_spin_is_locked(lock)) {
      continue;
    }
    if (arch_spin_trylock(lock)) {
      break;
    }
  }
}

arch_spinlock_t up_lock;
#else
#include <pthread.h>
pthread_mutex_t g_lock;
#endif // __riscv

// Source: https://github.com/yilongli/RAMCloud/blob/millisort/src/MilliSortService.cc#L769
void partition(uint64_t keys[], int numKeys, uint64_t pivots[], int numPartitions) {
    // Let P be the number of partitions. Pick P pivots that divide #keys into
    // P partitions as evenly as possible. Since the sizes of any two partitions
    // differ by at most one, we have:
    //      s * k + (s + 1) * (P - k) = numKeys
    // where s is the number of keys in smaller partitions and k is the number
    // of smaller partitions.
    int s = numKeys / numPartitions;
    int k = numPartitions * (s + 1) - numKeys;
    int pivotIdx = -1;
    for (int i = 0; i < numPartitions; i++) {
        if (i < k) {
            pivotIdx += s;
        } else {
            pivotIdx += s + 1;
        }
        pivots[i] = keys[pivotIdx];
    }
}


void send_ready_msg(int cid, uint64_t context_id) {
  uint64_t app_hdr = (uint64_t)SWITCH_IP << 32 | (0 << 16) | (2*8);
  lnic_write_r(app_hdr);
  lnic_write_r(cid);
  lnic_write_r(context_id);
}

void recv_start_msg() {
  lnic_wait();
  uint64_t app_hdr = lnic_read();
  uint16_t msg_len = (uint16_t)app_hdr;
  uint32_t src_ip = (app_hdr & IP_MASK) >> 32;
  if (src_ip != SWITCH_IP) printf("Error: expected start message from SWITCH_IP (src_ip=0x%x, msg_len=%d)\n", src_ip, msg_len);
  for (int w = 0; w < msg_len/8; w++) lnic_read();
  lnic_msg_done();
}


void send_keys(uint64_t metadata, unsigned dst_node_id, uint64_t *keys, unsigned key_cnt) {
  myassert(dst_node_id < GLOBAL_M);
  myassert(g_node_addrs[dst_node_id]);
  unsigned send_cnt, sent = 0;
  do {
    unsigned remaining = key_cnt - sent;
    if (remaining > MAX_VALS_PER_PKT) send_cnt = MAX_VALS_PER_PKT;
    else { // this is the last segment
      send_cnt = remaining;
      metadata |= FLAG_LAST;
    }
    uint16_t msg_len = (2 + 1 + send_cnt) * 8;
    uint64_t app_hdr = g_node_addrs[dst_node_id] | msg_len;
    //printf("sending to %d key_cnt=%u msg_len=%d app_hdr=0x%lx\n", dst_node_id, key_cnt, msg_len, app_hdr);
    lnic_write_r(app_hdr);
    lnic_write_i(0); // service_time
    lnic_write_i(0); // sent_time
    lnic_write_r(metadata);
    for (unsigned i = sent; i < sent+send_cnt; i++)
      lnic_write_r(keys[i]);
    sent += send_cnt;
  } while (sent < key_cnt);
}

void send_item(uint64_t metadata, uint64_t dst_node_id, uint64_t key, uint64_t *value) {
  myassert(dst_node_id < GLOBAL_M);
  myassert(g_node_addrs[dst_node_id]);
  uint16_t msg_len = (value == NULL ? (2 + 1) : ITEM_MSG_SIZE_WORDS) * 8;
  uint64_t app_hdr = g_node_addrs[dst_node_id] | msg_len;
  metadata |= MSG_TYPE_ITEM;
  lnic_write_r(app_hdr);
  lnic_write_i(0); // service_time
  lnic_write_i(0); // sent_time
  lnic_write_r(metadata);
  if (value == NULL) return;
  lnic_write_r(key);
#if VALUE_SIZE_WORDS != 12
#error "This unrolled loop does not match VALUE_SIZE_WORDS"
#endif
  lnic_write_m(*(value + 0));
  lnic_write_m(*(value + 1));
  lnic_write_m(*(value + 2));
  lnic_write_m(*(value + 3));
  lnic_write_m(*(value + 4));
  lnic_write_m(*(value + 5));
  lnic_write_m(*(value + 6));
  lnic_write_m(*(value + 7));
  lnic_write_m(*(value + 8));
  lnic_write_m(*(value + 9));
  lnic_write_m(*(value + 10));
  lnic_write_m(*(value + 11));
}

#define RECV_BUFF_SIZE (GLOBAL_M * 3)
struct recv_buffer {
  int head_for_type[MAX_MSG_TYPE+1];
  int tail_for_type[MAX_MSG_TYPE+1];
  uint32_t src_for_type[MAX_MSG_TYPE+1][RECV_BUFF_SIZE];
  uint16_t cnt_for_type[MAX_MSG_TYPE+1][RECV_BUFF_SIZE];
  uint64_t vals_for_type[MAX_MSG_TYPE+1][RECV_BUFF_SIZE][MAX_VALS_PER_PKT];
  uint16_t meta_for_type[MAX_MSG_TYPE+1][RECV_BUFF_SIZE];
};

bool recv_buf_empty(struct recv_buffer *rb) {
  bool is_empty = true;
  for (unsigned msg_type = 1; msg_type < MAX_MSG_TYPE+1; msg_type++)
    if (rb->head_for_type[msg_type] != rb->tail_for_type[msg_type]) {
      printf("msg_type=%d count=%d\n", msg_type, rb->head_for_type[msg_type] - rb->tail_for_type[msg_type]);
      is_empty = false;
    }
  return is_empty;
}

// Returns true iff a packet was found
bool recv_already_enqueued(int exp_msg_type, uint32_t *src_nid, uint64_t *metadata, unsigned *cnt, uint64_t *vals, struct recv_buffer *rb) {
  unsigned recv_cnt;
  if (rb->head_for_type[exp_msg_type] != rb->tail_for_type[exp_msg_type]) {
    recv_cnt = rb->cnt_for_type[exp_msg_type][rb->head_for_type[exp_msg_type]];
    for (unsigned i = 0; i < recv_cnt; i++) {
      vals[i] = rb->vals_for_type[exp_msg_type][rb->head_for_type[exp_msg_type]][i];
    }
    if (metadata != NULL) *metadata = rb->meta_for_type[exp_msg_type][rb->head_for_type[exp_msg_type]];
    rb->head_for_type[exp_msg_type] = (rb->head_for_type[exp_msg_type] + 1) % RECV_BUFF_SIZE;
    if (cnt != NULL) *cnt = recv_cnt;
    if (src_nid != NULL) *src_nid = rb->src_for_type[exp_msg_type][rb->head_for_type[exp_msg_type]];
    return true;
  }
  return false;
}

// Returns the metadata received in the packet
uint64_t recv_msg(int exp_msg_type, uint32_t *src_nid, unsigned *cnt, uint64_t *vals, struct recv_buffer *rb) {
  uint64_t metadata;
  uint32_t recv_src_nid;
  unsigned recv_cnt;
  if (!recv_already_enqueued(exp_msg_type, &recv_src_nid, &metadata, &recv_cnt, vals, rb)) {
    bool found = false;
    while (!found) {
      lnic_wait();
      uint64_t app_hdr = lnic_read();
      uint16_t msg_len = (uint16_t)app_hdr;
      uint16_t src_ctx = (uint16_t)(app_hdr >> 16);
      uint32_t src_ip = (uint32_t)(app_hdr >> 32);
      recv_src_nid = 4*(src_ip - BASE_NODE_IP) + src_ctx;
      lnic_read(); // service_time
      lnic_read(); // sent_time
      metadata = lnic_read();
      uint8_t msg_type = (uint8_t)metadata;
      if (msg_type > MAX_MSG_TYPE) printf("Bad msg_type=%d\n", msg_type);
      //if (msg_type == MSG_TYPE_ITEM) printf("Error: I cannot buffer MSG_TYPE_ITEM\n");
      if (msg_type == MSG_TYPE_L1_PIVOTS && msg_len != L1_PIVOTS_MSG_SIZE_WORDS*8) printf("Error: expected msg_type=%d to have msg_len %d but got %d\n", msg_type, L1_PIVOTS_MSG_SIZE_WORDS * 8, msg_len);
      if (msg_type == MSG_TYPE_L2_PIVOTS && msg_len != L2_PIVOTS_MSG_SIZE_WORDS*8) printf("Error: expected msg_type=%d to have msg_len %d but got %d\n", msg_type, L2_PIVOTS_MSG_SIZE_WORDS * 8, msg_len);
      if (msg_type == MSG_TYPE_L2_SPLITTERS && msg_len != L2_SPLITTERS_MSG_SIZE_WORDS*8) printf("Error: expected msg_type=%d to have msg_len %d but got %d\n", msg_type, L2_SPLITTERS_MSG_SIZE_WORDS * 8, msg_len);

      recv_cnt = (msg_len/8) - (2 + 1);

      if (recv_cnt > MAX_VALS_PER_PKT) printf("Error: packet too big (msg_type=%d recv_cnt=%d MAX_VALS_PER_PKT=%d)\n", msg_type, recv_cnt, MAX_VALS_PER_PKT);
      myassert(recv_cnt <= MAX_VALS_PER_PKT && "received more values than I can handle");

      if (msg_type == exp_msg_type) {
        for (unsigned i = 0; i < recv_cnt; i++)
          vals[i] = lnic_read();
        found = true;
      }
      else {
        for (unsigned i = 0; i < recv_cnt; i++) {
          rb->vals_for_type[msg_type][rb->tail_for_type[msg_type]][i] = lnic_read();
        }
        rb->src_for_type[msg_type][rb->tail_for_type[msg_type]] = recv_src_nid;
        rb->cnt_for_type[msg_type][rb->tail_for_type[msg_type]] = recv_cnt;
        rb->meta_for_type[msg_type][rb->tail_for_type[msg_type]] = metadata;
        rb->tail_for_type[msg_type] = (rb->tail_for_type[msg_type] + 1) % RECV_BUFF_SIZE;
        if (rb->tail_for_type[msg_type] == rb->head_for_type[msg_type]) printf("Error: ring buffer overflow!!!!!!!!!!!!!!!!\n");
      }
      lnic_msg_done();
    }
  }
  if (cnt != NULL) *cnt = recv_cnt;
  if (src_nid != NULL) *src_nid = recv_src_nid;
  return metadata;
}

struct timing {
  unsigned sort_original;
  unsigned recv_l1pivots;
  unsigned select_l2pivots;
  unsigned recv_l2pivots;
  unsigned send_l2pivots;
  unsigned recv_l2splitters;
  unsigned shuffle_l1pivots;
  unsigned sort_l1pivots;
  unsigned send_l1splitters;
  unsigned partition;
  unsigned final_shuffle;
  unsigned final_sort;
  unsigned rearrange;
  unsigned elapsed;
};

struct sorter_state {
  unsigned cid;
  unsigned nid;
  khash_t(m64) *idx_for_orig_key;
  khash_t(m64) *idx_for_final_key;
  uint64_t original_keys[KEYS_PER_NODE];
  uint64_t original_values[KEYS_PER_NODE][VALUE_SIZE_WORDS];
  uint64_t recvd_keys[MAX_KEYS_PER_NODE];
  unsigned recvd_keys_cnt;
  uint64_t final_values[MAX_KEYS_PER_NODE][VALUE_SIZE_WORDS];
  struct recv_buffer rb;
  volatile bool finished;
  struct timing times;
};
#ifdef __riscv
struct sorter_state g_ss[NCORES];
#else
struct sorter_state g_ss[NCORES * NNODES];
#endif

void destroy_core_state(struct sorter_state *ss) {
  kh_destroy(m64, ss->idx_for_orig_key);
  kh_destroy(m64, ss->idx_for_final_key);
}

void init_core_state(struct sorter_state *ss, unsigned nid) {
  memset(&ss->rb.head_for_type, 0, sizeof(ss->rb.head_for_type));
  memset(&ss->rb.tail_for_type, 0, sizeof(ss->rb.tail_for_type));
  memset(&ss->times, 0, sizeof(ss->times));
  ss->nid = nid;
  ss->finished = false;

  int kret;
  ss->idx_for_orig_key = kh_init(m64);
  kret = kh_resize(m64, ss->idx_for_orig_key, KEYS_PER_NODE+1);
  myassert(kret == 0);
  ss->idx_for_final_key = kh_init(m64);
  kret = kh_resize(m64, ss->idx_for_final_key, MAX_KEYS_PER_NODE);
  myassert(kret == 0);
}


int run_node(unsigned cid, uint64_t context_id, unsigned nid) {
  if (nid >= NNODES*NCORES) {
    send_ready_msg(cid, context_id);
    recv_start_msg();
    return EXIT_SUCCESS;
  }
#ifdef __riscv
  struct sorter_state *ss = &g_ss[cid];
  arch_spin_lock(&up_lock);
  init_core_state(ss, nid);
  arch_spin_unlock(&up_lock);
#else
  struct sorter_state *ss = &g_ss[nid];
  pthread_mutex_lock(&g_lock);
  init_core_state(ss, nid);
  pthread_mutex_unlock(&g_lock);
#endif
  //if (cid == 0) printf("[%d] Starting sort... g_total_alloc=%u\n", ss->nid, g_total_alloc);

  // Generate the keys for this node
  for (unsigned i = 0; i < KEYS_PER_NODE; i++)
#if 1
    ss->original_keys[i] = nid*KEYS_PER_NODE + i+1;
#else
    ss->original_keys[i] = rand();
#endif

  send_ready_msg(cid, context_id);
  recv_start_msg();
  g_did_init = true;

  bool isPivotSorter = (nid % NODES_PER_PIVOT_SORTER) == 0;
  unsigned myPivotSorterIdx = nid / NODES_PER_PIVOT_SORTER;
  unsigned myPivotSorterId = myPivotSorterIdx * NODES_PER_PIVOT_SORTER;
  bool isCoordinator = nid == 0;
  unsigned key_cnt;
  int kret;
  khint_t kk;

#define NUM_L1_PIVOTS_PER_SORTER (NODES_PER_PIVOT_SORTER * NUM_L1_PIVOTS)
#define MAX_L1_PIVOTS_PER_SORTER (NUM_L1_PIVOTS_PER_SORTER * 3)

  for (unsigned idx = 0; idx < KEYS_PER_NODE; idx++) {
    kk = kh_put(m64, ss->idx_for_orig_key, ss->original_keys[idx], &kret);
    myassert(kret > 0);
    kh_value(ss->idx_for_orig_key, kk) = idx;
  }

  uint64_t time_a = rdcycle(), time_b;
  uint64_t sort_start = time_a;

  isort(ss->original_keys, KEYS_PER_NODE); // Sort my initial keys

  ss->times.sort_original = (time_b = rdcycle()) - time_a;
  uint64_t partition_start = time_b;

  uint64_t l1_pivots[NUM_L1_PIVOTS_PER_SORTER];
  partition(ss->original_keys, KEYS_PER_NODE, l1_pivots, NUM_L1_PIVOTS); // Pick level 1 pivots

  //pthread_mutex_lock(&g_lock);
  //printf("[%d] original keys:", nid); printArr(ss->original_keys, KEYS_PER_NODE);
  //printf("[%d] my l1 pivots:", nid); printArr(l1_pivots, NUM_L1_PIVOTS);
  //pthread_mutex_unlock(&g_lock);

  uint64_t l1_splitters_from_sorter[NUM_PIVOT_SORTERS][MAX_L1_SPLITTERS_PER_NODE];
  uint16_t l1_splitters_from_sorter_cnt[NUM_PIVOT_SORTERS];

  if (isPivotSorter) { // Receive pivots
    unsigned recvd_l1_pivots = NUM_L1_PIVOTS;
    for (unsigned recv_cnt = 1; recv_cnt < NODES_PER_PIVOT_SORTER;) {
      uint64_t md = recv_msg(MSG_TYPE_L1_PIVOTS, NULL, &key_cnt, &l1_pivots[recvd_l1_pivots], &ss->rb);
      myassert(key_cnt == NUM_L1_PIVOTS);
      if (md & FLAG_LAST) recv_cnt++;
      recvd_l1_pivots += key_cnt;
    }

    ss->times.recv_l1pivots = (time_a = rdcycle()) - time_b;

    uint64_t l2_pivots[NUM_PIVOT_SORTERS * NUM_L2_PIVOTS];
    isort(l1_pivots, NUM_L1_PIVOTS_PER_SORTER);
    partition(l1_pivots, NUM_L1_PIVOTS_PER_SORTER, l2_pivots, NUM_L2_PIVOTS);

    ss->times.select_l2pivots = (time_b = rdcycle()) - time_a;

    uint64_t l2_splitters[NUM_L2_SPLITTERS];

    if (isCoordinator) { // Gather l2 pivots
      unsigned recvd_l2_pivots = NUM_L2_PIVOTS;
      for (unsigned recv_cnt = 1; recv_cnt < NUM_PIVOT_SORTERS;) {
        uint64_t md = recv_msg(MSG_TYPE_L2_PIVOTS, NULL, &key_cnt, &l2_pivots[recvd_l2_pivots], &ss->rb);
        if (md & FLAG_LAST) recv_cnt++;
        recvd_l2_pivots += key_cnt;
      }

      ss->times.recv_l2pivots = (time_a = rdcycle()) - time_b;

      isort(l2_pivots, NUM_PIVOT_SORTERS*NUM_L2_PIVOTS); // Sort l2 pivots
      partition(l2_pivots, NUM_PIVOT_SORTERS*NUM_L2_PIVOTS, l2_splitters, NUM_L2_SPLITTERS); // Select l2 splitters

      //printf("[%d] l2_splitters:", nid); printArr(l2_splitters, NUM_L2_SPLITTERS);

      // Multicast l2 splitters
      for (int dst_node = NODES_PER_PIVOT_SORTER; dst_node < GLOBAL_M; dst_node += NODES_PER_PIVOT_SORTER)
        send_keys(MSG_TYPE_L2_SPLITTERS, dst_node, l2_splitters, NUM_L2_SPLITTERS);
      ss->times.send_l2pivots = (time_b = rdcycle()) - time_a; time_a = time_b;
    }
    else {
      // Send l2 pivots to pivot coordinator
      unsigned coordinatorId = 0;
      send_keys(MSG_TYPE_L2_PIVOTS, coordinatorId, l2_pivots, NUM_L2_PIVOTS);

      // Receive l2 splitters from coordinator
      unsigned recvd_l2_splitters = 0;
      while (recvd_l2_splitters < NUM_L2_SPLITTERS) {
        recv_msg(MSG_TYPE_L2_SPLITTERS, NULL, &key_cnt, l2_splitters, &ss->rb);
        recvd_l2_splitters += key_cnt;
      }
      ss->times.recv_l2splitters = (time_a = rdcycle()) - time_b;
    }

    // Shuffle l1 pivots with l2 splitters
    unsigned recvd_pivot_cnt = 0;
    uint64_t l1_pivots2[MAX_L1_PIVOTS_PER_SORTER];
    unsigned my_rank = 0;
    unsigned prev_piv_idx = 0;

    // Split my l1 pivots and send them to other pivot sorters
    for (unsigned spl_idx = 0; spl_idx < NUM_L2_SPLITTERS; spl_idx++) {
      unsigned piv_cnt;
      for (piv_cnt = 0; prev_piv_idx+piv_cnt < NUM_L1_PIVOTS_PER_SORTER; piv_cnt++) {
        if (l1_pivots[prev_piv_idx + piv_cnt] > l2_splitters[spl_idx]) break;
      }
      if (prev_piv_idx + piv_cnt == NUM_L1_PIVOTS_PER_SORTER) piv_cnt = NUM_L1_PIVOTS_PER_SORTER - prev_piv_idx;

      unsigned dstPivotSorterId = spl_idx * NODES_PER_PIVOT_SORTER;
      //printf("NUM_L2_SPLITTERS=%u spl_idx=%d dstPivotSorterId=%u\n", NUM_L2_SPLITTERS, spl_idx, dstPivotSorterId);
      //printf("[%d] dstPivotSorterId=%u piv_cnt=%u prev_piv_idx=%u\n", nid, dstPivotSorterId, piv_cnt, prev_piv_idx);
      if (dstPivotSorterId == nid) { // if it's me, just copy to myself
        memcpy(l1_pivots2, &l1_pivots[prev_piv_idx], piv_cnt*sizeof(uint64_t));
        recvd_pivot_cnt += piv_cnt;
        my_rank += prev_piv_idx;
      }
      else  {
        uint64_t md = SET_RANK(prev_piv_idx);
        send_keys(md | MSG_TYPE_SHUF_PIVOTS, dstPivotSorterId, &l1_pivots[prev_piv_idx], piv_cnt);
      }

      prev_piv_idx += piv_cnt;
    }

    // Receive l1 pivots from other pivot sorters
    for (unsigned recv_cnt = 1; recv_cnt < NUM_PIVOT_SORTERS;) {
      uint64_t md = recv_msg(MSG_TYPE_SHUF_PIVOTS, NULL, &key_cnt, &l1_pivots2[recvd_pivot_cnt], &ss->rb);
      recvd_pivot_cnt += key_cnt;
      myassert(recvd_pivot_cnt <= MAX_L1_PIVOTS_PER_SORTER);
      if (md & FLAG_LAST) {
        my_rank += GET_RANK(md);
        recv_cnt++;
      }
    }
    ss->times.shuffle_l1pivots = (time_b = rdcycle()) - time_a;
    isort(l1_pivots2, recvd_pivot_cnt);
    ss->times.sort_l1pivots = (time_a = rdcycle()) - time_b;


    // Select l1 splitters
    unsigned my_l1_splitters_cnt = 0;
    int totalPivots = NUM_L1_PIVOTS * GLOBAL_M;
    int s = totalPivots / GLOBAL_M;
    int k = GLOBAL_M * (s + 1) - totalPivots;
    int globalIdx = -1;
    for (int i = 0; i < GLOBAL_M; i++) {
      if (i < k) globalIdx += s;
      else       globalIdx += s + 1;
      int localIdx = globalIdx - my_rank;
      if ((0 <= localIdx) && (localIdx < (int) recvd_pivot_cnt))
        l1_splitters_from_sorter[myPivotSorterIdx][my_l1_splitters_cnt++] = l1_pivots2[localIdx];
    }
    //printf("[%d] my_rank=%u recvd_pivot_cnt=%u my_l1_splitters_cnt=%u\n", nid, my_rank, recvd_pivot_cnt, my_l1_splitters_cnt);
    l1_splitters_from_sorter_cnt[myPivotSorterIdx] = my_l1_splitters_cnt;

    //if (nid <= 0) {
    //  printf("[%d] l1_pivots2:", nid); printArr(l1_pivots2, recvd_pivot_cnt);
    //  printf("[%d] my_l1_splitters:", nid); printArr(my_l1_splitters, my_l1_splitters_cnt);
    //}

    // Multicast my l1 splitters
    for (unsigned dst_node = 0; dst_node < GLOBAL_M; dst_node++) {
      if (dst_node == nid) continue; // don't send to myself
      send_keys(MSG_TYPE_L1_SPLITTERS, dst_node, l1_splitters_from_sorter[myPivotSorterIdx], my_l1_splitters_cnt);
    }
    ss->times.send_l1splitters = (time_b = rdcycle()) - time_a;
  }
  else { // Send l1 pivots to pivot sorter
    send_keys(MSG_TYPE_L1_PIVOTS, myPivotSorterId, l1_pivots, NUM_L1_PIVOTS);
  }

  uint64_t l1_splitters[NUM_L1_SPLITTERS];
  uint64_t tmp_l1_splitters[MAX_L1_PIVOTS_PER_SORTER];
  // Receive l1 splitters from each pivot sorter (except myself if I'm a pivot sorter)
  for (unsigned recv_cnt = isPivotSorter ? 1 : 0; recv_cnt < NUM_PIVOT_SORTERS;) {
    uint32_t src_nid;
    uint64_t md = recv_msg(MSG_TYPE_L1_SPLITTERS, &src_nid, &key_cnt, tmp_l1_splitters, &ss->rb);
    memcpy(l1_splitters_from_sorter[PIV_SORTER_IDX(src_nid)], tmp_l1_splitters, key_cnt*sizeof(uint64_t));
    l1_splitters_from_sorter_cnt[PIV_SORTER_IDX(src_nid)] = key_cnt;
    if (md & FLAG_LAST) recv_cnt++;
  }
  unsigned recvd_l1_splitters_cnt = 0;

  unsigned spl_idx = 0;
  for (unsigned i = 0; i < NUM_PIVOT_SORTERS; i++) {
    memcpy(&l1_splitters[spl_idx], l1_splitters_from_sorter[i], l1_splitters_from_sorter_cnt[i]*sizeof(uint64_t));
    spl_idx += l1_splitters_from_sorter_cnt[i];
    myassert(recvd_l1_splitters_cnt <= NUM_L1_SPLITTERS);
  }
  myassert(spl_idx == NUM_L1_SPLITTERS);
  ss->times.partition = (time_a = rdcycle()) - partition_start;

  //pthread_mutex_lock(&g_lock);
  //if (nid == 0) { printf("[%d] l1_splitters:", nid); printArr(l1_splitters, NUM_L1_SPLITTERS); }
  //pthread_mutex_unlock(&g_lock);

  unsigned key_idx = 0;
  for (unsigned dst_node = 0; dst_node < GLOBAL_M; dst_node++) { // Send keys to final nodes
    unsigned send_cnt = 0;
    while (key_idx + send_cnt < KEYS_PER_NODE && ss->original_keys[key_idx + send_cnt] <= l1_splitters[dst_node]) send_cnt++;
    for (unsigned i = 0; i < send_cnt; i++) {
      uint64_t md = i+1 == send_cnt ? FLAG_LAST : 0;
      uint64_t *key = &ss->original_keys[key_idx + i];
      kk = kh_get(m64, ss->idx_for_orig_key, *key);
      myassert(kk != kh_end(ss->idx_for_orig_key) && "I should know where my original keys are");
      unsigned val_idx = kh_value(ss->idx_for_orig_key, kk);
      send_item(md, dst_node, *key, ss->original_values[val_idx]);
    }
    if (send_cnt == 0)
      send_item(FLAG_LAST | FLAG_NULL, dst_node, 0, NULL);
    key_idx += send_cnt;
  }

  // we read the entire message into here, so leave some extra space for the key:
  uint64_t tmp_values[MAX_KEYS_PER_NODE][1 + VALUE_SIZE_WORDS];

  ss->recvd_keys_cnt = 0;
  for (unsigned recv_cnt = 0; recv_cnt < GLOBAL_M;) { // Receive final keys
    unsigned cnt;
    uint64_t md = recv_msg(MSG_TYPE_ITEM, NULL, &cnt, tmp_values[ss->recvd_keys_cnt], &ss->rb);
    myassert(ss->recvd_keys_cnt + cnt <= MAX_KEYS_PER_NODE);
    if (md & FLAG_NULL) {
      myassert(cnt == 0);
    } else {
      myassert(cnt == 1 + VALUE_SIZE_WORDS);
      uint64_t *key = &tmp_values[ss->recvd_keys_cnt][0];
      ss->recvd_keys[ss->recvd_keys_cnt] = *key;
      kk = kh_put(m64, ss->idx_for_final_key, *key, &kret);
      myassert(kret > -1);
      kh_value(ss->idx_for_final_key, kk) = ss->recvd_keys_cnt++;
    }
    if (md & FLAG_LAST) recv_cnt++;
  }

  ss->times.final_shuffle = (time_b = rdcycle()) - time_a;

  isort(ss->recvd_keys, ss->recvd_keys_cnt); // Sort final keys

  ss->times.final_sort = (time_a = rdcycle()) - time_b;

  for (unsigned i = 0; i < ss->recvd_keys_cnt; i++) { // Rearrange values
    kk = kh_get(m64, ss->idx_for_final_key, ss->recvd_keys[i]);
    myassert(kk != kh_end(ss->idx_for_final_key) && "I should have the idx for all received final keys");
    memcpy(ss->final_values[i], &tmp_values[kh_value(ss->idx_for_final_key, kk)][1], VALUE_SIZE_WORDS*sizeof(uint64_t));
  }

  ss->times.rearrange = (time_b = rdcycle()) - time_a;

  ss->times.elapsed = time_b - sort_start;

  destroy_core_state(ss);

  ss->finished = true;

#ifdef __riscv
#define FIRST_CORE cid
#define NCORES_IN_COMP_UNIT NCORES
#else
#define FIRST_CORE ss->nid
#define NCORES_IN_COMP_UNIT (NNODES*NCORES)
#endif
  if (FIRST_CORE == 0) {
    for (unsigned i = 0; i < NCORES_IN_COMP_UNIT; i++) while (!g_ss[i].finished) { }
    uint64_t prev_max = g_ss[0].recvd_keys_cnt < 1 ? 0 : g_ss[0].recvd_keys[g_ss[0].recvd_keys_cnt-1];
    // Check that the nodes are sorted
    for (unsigned i = 1; i < NCORES_IN_COMP_UNIT; i++) {
      if (g_ss[i].recvd_keys_cnt == 0) continue;
      uint64_t *this_max = &g_ss[i].recvd_keys[0],
               *this_min = &g_ss[i].recvd_keys[g_ss[i].recvd_keys_cnt-1];
      myassert(prev_max <= *this_min);
      prev_max = *this_max;
    }

    for (unsigned i = 0; i < NCORES_IN_COMP_UNIT; i++) {
      struct timing *t = &g_ss[i].times;
      printf("TimingCSV: %u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u\n", i, g_ss[i].recvd_keys_cnt,
          t->sort_original, t->recv_l1pivots, t->select_l2pivots, t->recv_l2pivots,
          t->send_l2pivots, t->recv_l2splitters, t->shuffle_l1pivots, t->sort_l1pivots,
          t->send_l1splitters, t->partition, t->final_shuffle, t->final_sort, t->rearrange, t->elapsed);
    }
#ifndef __riscv
    unsigned total_keys = 0;
    for (unsigned i = 0; i < NCORES_IN_COMP_UNIT; i++) total_keys += g_ss[i].recvd_keys_cnt;
    myassert(total_keys == KEYS_PER_NODE*GLOBAL_M);
    float skews[NCORES_IN_COMP_UNIT];
    for (unsigned i = 0; i < NCORES_IN_COMP_UNIT; i++) skews[i] = g_ss[i].recvd_keys_cnt / (float)KEYS_PER_NODE;
    for (unsigned i = 0; i < NCORES_IN_COMP_UNIT; i++) { printf(" %.2f", skews[i]); } printf("\n");
    std::sort(std::begin(skews), std::end(skews));
    for (unsigned i = 0; i < NCORES_IN_COMP_UNIT; i++) { printf(" %.2f", skews[i]); } printf("\n");
#endif
    for (int i = 0; i < 100000; i++) asm volatile("nop"); // wait for other nodes to finish
  }

  return EXIT_SUCCESS;
}


// These are defined in syscalls.c
int inet_pton4(const char *src, const char *end, unsigned char *dst);
uint32_t swap32(uint32_t in);

bool is_single_core() { return false; }

int core_main(int argc, char** argv, int cid, int nc) {
  (void)nc;
  //printf("args: "); for (int i = 1; i < argc; i++) printf("%s ", argv[i]); printf("\n");

  if (argc < 3) {
      printf("This program requires passing the L-NIC MAC address, followed by the L-NIC IP address.\n");
      return -1;
  }

  char* nic_ip_str = argv[2];
  uint32_t nic_ip_addr_lendian = 0;
  int retval = inet_pton4(nic_ip_str, nic_ip_str + strlen(nic_ip_str), (unsigned char *)&nic_ip_addr_lendian);

  // Risc-v is little-endian, but we store ip's as big-endian since the NIC works in big-endian
  uint32_t nic_ip_addr = swap32(nic_ip_addr_lendian);
  if (retval != 1 || nic_ip_addr == 0) {
      printf("Supplied NIC IP address is invalid.\n");
      return -1;
  }

  uint64_t context_id = cid;
  uint64_t priority = 0;
  lnic_add_context(context_id, priority);

  int nanopu_node_id = nic_ip_addr - BASE_NODE_IP;
  int sorter_node_id = (nanopu_node_id * nc) + cid;
#ifdef __riscv
  if (cid == 0)
#else
  if (sorter_node_id == 0)
#endif
  {
    for (unsigned i = 0; i < GLOBAL_M; i++) {
      g_node_addrs[i] = (uint64_t)(BASE_NODE_IP + (i/4) ) << 32 | (i%4) << 16;
    }
    g_seed = nanopu_node_id;
  }

  return run_node(cid, context_id, sorter_node_id);
}

#ifdef __cplusplus
}
#endif
