#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <string.h>
#include <stdbool.h>
#include "lnic.h"

#ifdef __riscv
#define myassert(x) if (!(x)) printf("ASSERTION FAILED: %s:%d %s()\n", __FILE__, __LINE__,  __func__)
#else
#include <assert.h>
#define myassert assert
#endif

static unsigned g_seed = 0;
static inline int fastrand() { // Source: https://stackoverflow.com/a/3747462
  g_seed = (214013*g_seed+2531011);
  //return (g_seed>>16)&0x7FFF;
  return g_seed >> 1;
}
#define rand fastrand

inline int mypow(int a, int b) { // a**b
  int x = 1;
  for (int i = 0; i < b; i++) x *= a;
  return x;
}
#define pow mypow


volatile bool g_did_init = false;

#define USE_MYHEAP 1
#define MYHEAP_SIZE (32 * 1024)
#if USE_MYHEAP
char g_myheap[MYHEAP_SIZE];
int g_myheap_head = 0;
#endif

unsigned g_total_alloc = 0;
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
#define kmalloc mymalloc
void *mycalloc(size_t a, size_t b) {
  g_total_alloc += a*b;
#if USE_MYHEAP
  void *p = mymalloc(a*b);
#else
  void *p = calloc(a, b);
#endif
  if (g_did_init) printf("calloc(%ld): %p\n", a*b, p);
  return p;
}
#define kcalloc mycalloc
void myfree(void *p) {
  //if (g_did_init) printf("free(%p)\n", p);
#if USE_MYHEAP
#else
  return free(p);
#endif
}
#define kfree myfree
void *myrealloc(void *p, size_t sz) {
  g_total_alloc += sz;
#if USE_MYHEAP
  if (sz == 0) { myfree(p); return NULL; }
  char *p2 = (char *)mymalloc(sz);
  if (p != NULL) memcpy(p2, p, sz);
#else
  void *p2 = realloc(p, sz);
#endif
  if (g_did_init) printf("realloc(%p, %ld): %p\n", p, sz, p2);
  return p2;
}
#define krealloc myrealloc

#ifndef drand48
#define drand48() ((rand() % 999999)/(float)999999)
#endif

#include "khash.h"
KHASH_MAP_INIT_INT64(m64, unsigned)
KHASH_MAP_INIT_INT64(m64vec, struct simple_vector *)
#include "ksort.h"
KSORT_INIT_GENERIC(uint64_t)
KSORT_INIT_GENERIC(int)

#include "qsort.h"
void isort(uint64_t A[], size_t n) {
  uint64_t tmp;
#define LESS(i, j) A[i] < A[j]
#define SWAP(i, j) tmp = A[i], A[i] = A[j], A[j] = tmp
  QSORT(n, LESS, SWAP);
}

void fsort(float A[], size_t n) {
  float tmp;
#define LESS(i, j) A[i] < A[j]
#define SWAP(i, j) tmp = A[i], A[i] = A[j], A[j] = tmp
  QSORT(n, LESS, SWAP);
}

#define MAX(a,b) __extension__({ __typeof__ (a) _a = (a);  __typeof__ (b) _b = (b); _a > _b ? _a : _b; })
#define MIN(a,b) __extension__({ __typeof__ (a) _a = (a);  __typeof__ (b) _b = (b); _a < _b ? _a : _b; })


#define NCORES 4
#define NNODES 8

#define GLOBAL_M                (NCORES * NNODES)
#define GLOBAL_N                8
#define MAX_KEYS_PER_NODE       (GLOBAL_N*8)
#define MAX_KEYS_PER_PKT        32
#define VALUE_SIZE_WORDS        12

#define MSG_TYPE_STEP0      1
#define MSG_TYPE_STEP1_KTH  2
#define MSG_TYPE_STEP2      3 // Only used for OPTION 2 (median of medians)
#define MSG_TYPE_STEP3      4
#define MSG_TYPE_STEP4      5
#define MSG_TYPE_SHUFL_REQ  6
#define MSG_TYPE_SHUFL_RES  7

#define MAX_MSG_TYPES 7

#define KEY_MSG_SIZE_WORDS (2 + 1 + 1) // {msg_type, key}
#define KEYSRC_MSG_SIZE_WORDS (2 + 1 + 1 + 1) // {msg_type, key, orig_node}
#define ITEM_MSG_SIZE_WORDS (2 + 1 + 1 + VALUE_SIZE_WORDS) // {msg_type, key, value}

#define FLAG_NULL           (1 << 0)

#define BASE_NODE_IP 0x0a000002

int g_shuffled_dst_node_ids[GLOBAL_M];
uint64_t g_dst_node_addrs[GLOBAL_M];

void printArr(uint64_t *a, int n) {
  for (int i = 0; i < n; i++) { printf(" %ld", a[i]); } printf("\n");
}
uint64_t maxArr(uint64_t *a, int n) {
  myassert(n > 0);
  uint64_t m = a[0];
  for (int i = 1; i < n; i++) if (a[i] > m) m = a[i];
  return m;
}
uint64_t minArr(uint64_t *a, int n) {
  myassert(n > 0);
  uint64_t m = a[0];
  for (int i = 1; i < n; i++) if (a[i] < m) m = a[i];
  return m;
}

struct simple_vector {
  unsigned size;
  uint64_t v[MAX_KEYS_PER_NODE];
};


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

#define SWITCH_IP 0x0a000001

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

void send_key_and_src(uint8_t msg_type, unsigned dst_node_id, uint64_t key, uint64_t orig_node) {
  uint16_t msg_len = KEYSRC_MSG_SIZE_WORDS * 8;
  myassert(dst_node_id < GLOBAL_M);
  uint64_t app_hdr = g_dst_node_addrs[dst_node_id] | msg_len;
  lnic_write_r(app_hdr);
  lnic_write_i(0); // service_time
  lnic_write_i(0); // sent_time
  lnic_write_r(msg_type);
  lnic_write_r(key);
  lnic_write_r(orig_node);
}

void send_key(uint8_t msg_type, unsigned dst_node_id, uint64_t key) {
  uint16_t msg_len = KEY_MSG_SIZE_WORDS * 8;
  myassert(dst_node_id < GLOBAL_M);
  uint64_t app_hdr = g_dst_node_addrs[dst_node_id] | msg_len;
  lnic_write_r(app_hdr);
  lnic_write_i(0); // service_time
  lnic_write_i(0); // sent_time
  lnic_write_r(msg_type);
  lnic_write_r(key);
}

void send_key_null(uint8_t msg_type, unsigned dst_node_id) {
  uint16_t msg_len = KEY_MSG_SIZE_WORDS * 8;
  myassert(dst_node_id < GLOBAL_M);
  uint64_t app_hdr = g_dst_node_addrs[dst_node_id] | msg_len;
  lnic_write_r(app_hdr);
  lnic_write_i(0); // service_time
  lnic_write_i(0); // sent_time
  lnic_write_r((FLAG_NULL << 8) | msg_type);
  lnic_write_i(0);
}

#define RECV_BUFF_SIZE GLOBAL_M
struct recv_buffer {
  int head_for_type[MAX_MSG_TYPES+1];
  int tail_for_type[MAX_MSG_TYPES+1];
  int cnt_for_type[MAX_MSG_TYPES+1][RECV_BUFF_SIZE];
  uint64_t keys_for_type[MAX_MSG_TYPES+1][RECV_BUFF_SIZE][MAX_KEYS_PER_PKT];
  uint64_t nodes_for_type[MAX_MSG_TYPES+1][RECV_BUFF_SIZE][MAX_KEYS_PER_PKT];
  uint16_t flags_for_type[MAX_MSG_TYPES+1][RECV_BUFF_SIZE];
};

// Returns true iff a packet was found
bool recv_already_enqueued(int exp_msg_type, uint8_t *flags, unsigned *key_cnt, uint64_t *keys, uint64_t *node_ids, struct recv_buffer *rb) {
  unsigned recv_key_cnt;
  if (rb->head_for_type[exp_msg_type] != rb->tail_for_type[exp_msg_type]) {
    recv_key_cnt = rb->cnt_for_type[exp_msg_type][rb->head_for_type[exp_msg_type]];
    for (unsigned i = 0; i < recv_key_cnt; i++) {
      keys[i] = rb->keys_for_type[exp_msg_type][rb->head_for_type[exp_msg_type]][i];
      if (exp_msg_type == MSG_TYPE_STEP0 || exp_msg_type == MSG_TYPE_STEP3 || exp_msg_type == MSG_TYPE_STEP4)
        node_ids[i] = rb->nodes_for_type[exp_msg_type][rb->head_for_type[exp_msg_type]][i];
    }
    if (exp_msg_type == MSG_TYPE_SHUFL_REQ)
      node_ids[0] = rb->nodes_for_type[exp_msg_type][rb->head_for_type[exp_msg_type]][0];
    if (flags != NULL) *flags = rb->flags_for_type[exp_msg_type][rb->head_for_type[exp_msg_type]];
    rb->head_for_type[exp_msg_type] = (rb->head_for_type[exp_msg_type] + 1) % RECV_BUFF_SIZE;
    if (key_cnt != NULL) *key_cnt = recv_key_cnt;
    return true;
  }
  return false;
}

// Returns the flags received in the packet
uint8_t recv_key(int exp_msg_type, unsigned *key_cnt, uint64_t *keys, uint64_t *node_ids, struct recv_buffer *rb) {
  uint8_t flags;
  unsigned recv_key_cnt;
  if (!recv_already_enqueued(exp_msg_type, &flags, &recv_key_cnt, keys, node_ids, rb)) {
    bool found = false;
    while (!found) {
      lnic_wait();
      uint64_t app_hdr = lnic_read();
      uint16_t msg_len = (uint16_t)app_hdr;
      lnic_read(); // service_time
      lnic_read(); // sent_time
      uint64_t sort_meta = lnic_read();
      uint8_t msg_type = (uint8_t)sort_meta;
      flags = sort_meta >> 8;
      if (msg_type > MAX_MSG_TYPES) printf("Bad msg_type=%d\n", msg_type);
      if (msg_type == MSG_TYPE_STEP0 || msg_type == MSG_TYPE_STEP3) {
        if (msg_len != KEYSRC_MSG_SIZE_WORDS * 8) printf("Error: expected msg_type=%d to have msg_len %d but got %d\n", msg_type, KEYSRC_MSG_SIZE_WORDS * 8, msg_len);
      }
      else if (msg_type == MSG_TYPE_STEP1_KTH || msg_type == MSG_TYPE_STEP2) {
        if (msg_len != KEY_MSG_SIZE_WORDS * 8) printf("Error: expected msg_type=%d to have msg_len %d but got %d\n", msg_type, KEY_MSG_SIZE_WORDS * 8, msg_len);
      }

      recv_key_cnt = 1;
      if (msg_type == MSG_TYPE_STEP4)
        recv_key_cnt = ((msg_len/8) - (2 + 1)) / 2;
      else if (msg_type == MSG_TYPE_SHUFL_REQ)
        recv_key_cnt = (msg_len/8) - (2 + 1);

      if (msg_type == MSG_TYPE_SHUFL_RES) printf("Error: I can't handdle MSG_TYPE_SHUFL_RES\n");
      if (recv_key_cnt > MAX_KEYS_PER_PKT) printf("Error: packet too big (recv_key_cnt=%d MAX_KEYS_PER_PKT=%d)\n", recv_key_cnt, MAX_KEYS_PER_PKT);
      myassert(recv_key_cnt <= MAX_KEYS_PER_PKT && "received more keys than I can handle");

      if (msg_type == exp_msg_type) {
        for (unsigned i = 0; i < recv_key_cnt; i++) {
          keys[i] = lnic_read();
          if (msg_type == MSG_TYPE_STEP0 || msg_type == MSG_TYPE_STEP3 || msg_type == MSG_TYPE_STEP4)
            node_ids[i] = lnic_read();
        }
        if (msg_type == MSG_TYPE_SHUFL_REQ) {
          uint64_t src_node_addr = app_hdr & (IP_MASK | CONTEXT_MASK);
          node_ids[0] = src_node_addr;
        }
        found = true;
      }
      else {
        for (unsigned i = 0; i < recv_key_cnt; i++) {
          rb->keys_for_type[msg_type][rb->tail_for_type[msg_type]][i] = lnic_read();
          if (msg_type == MSG_TYPE_STEP0 || msg_type == MSG_TYPE_STEP3 || msg_type == MSG_TYPE_STEP4)
            rb->nodes_for_type[msg_type][rb->tail_for_type[msg_type]][i] = lnic_read();
        }
        if (msg_type == MSG_TYPE_SHUFL_REQ) {
          uint64_t src_node_addr = app_hdr & (IP_MASK | CONTEXT_MASK);
          rb->nodes_for_type[msg_type][rb->tail_for_type[msg_type]][0] = src_node_addr;
        }
        rb->cnt_for_type[msg_type][rb->tail_for_type[msg_type]] = recv_key_cnt;
        rb->flags_for_type[msg_type][rb->tail_for_type[msg_type]] = flags;
        rb->tail_for_type[msg_type] = (rb->tail_for_type[msg_type] + 1) % RECV_BUFF_SIZE;
        if (rb->tail_for_type[msg_type] == rb->head_for_type[msg_type]) printf("Error: ring buffer overflow!!!!!!!!!!!!!!!!\n");
      }
      lnic_msg_done();
    }
  }
  if (key_cnt != NULL) *key_cnt = recv_key_cnt;
  return flags;
}

void send_item(uint64_t dst_node_addr, uint64_t key, uint64_t *value) {
  uint16_t msg_len = ITEM_MSG_SIZE_WORDS * 8;
  uint64_t app_hdr = dst_node_addr | msg_len;
  lnic_write_r(app_hdr);
  lnic_write_i(0); // service_time
  lnic_write_i(0); // sent_time
  lnic_write_r(MSG_TYPE_SHUFL_RES);
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

struct sorter_state {
  int cid;
  int nid;
  khash_t(m64vec) *orig_nodes_for_key;
  khash_t(m64) *idx_for_orig_key;
  khash_t(m64) *idx_for_final_key;
  khash_t(m64vec) *req_keys_from_node;
  uint64_t original_keys[GLOBAL_N];
  uint64_t original_values[GLOBAL_N * VALUE_SIZE_WORDS];
  uint64_t recvd_keys[MAX_KEYS_PER_NODE];
  unsigned recvd_keys_cnt;
  uint64_t final_values[MAX_KEYS_PER_NODE];
  struct recv_buffer rb;
  volatile bool finished;
#define VEC_POOL_SIZE (2 * MAX_KEYS_PER_NODE)
  struct simple_vector vec_pool[VEC_POOL_SIZE];
  struct simple_vector *vec_pool_head;
};
struct sorter_state g_ss[NCORES];

struct simple_vector *get_vec(struct sorter_state *ss) {
  myassert(ss->vec_pool_head != NULL && "vector pool is empty");
  struct simple_vector *v = ss->vec_pool_head;
  v->size = 0;
  ss->vec_pool_head = (struct simple_vector *)v->v[0];
  return v;
}

void free_vec(struct sorter_state *ss, struct simple_vector *v) {
  v->v[0] = (uint64_t)ss->vec_pool_head;
  ss->vec_pool_head = v;
}

void clear_nested_map(struct sorter_state *ss, khash_t(m64vec) *h) {
  khint_t kk;
  for (kk = kh_begin(h); kk != kh_end(h); ++kk) // traverse
    if (kh_exist(h, kk)) { // test if a bucket contains data
      free_vec(ss, kh_value(h, kk));
    }
  kh_clear(m64vec, h);
}

void destroy_core_state(struct sorter_state *ss) {
  kh_destroy(m64, ss->idx_for_orig_key);
  clear_nested_map(ss, ss->orig_nodes_for_key);
  kh_destroy(m64vec, ss->orig_nodes_for_key);
  kh_destroy(m64, ss->idx_for_final_key);
  clear_nested_map(ss, ss->req_keys_from_node);
  kh_destroy(m64vec, ss->req_keys_from_node);
}

void init_core_state(struct sorter_state *ss, int nid) {
  int kret;
  memset(&ss->rb.head_for_type, 0, sizeof(ss->rb.head_for_type));
  memset(&ss->rb.tail_for_type, 0, sizeof(ss->rb.tail_for_type));
  ss->nid = nid;
  ss->finished = false;

  ss->idx_for_orig_key = kh_init(m64);
  kret = kh_resize(m64, ss->idx_for_orig_key, GLOBAL_N*2);
  myassert(kret == 0);
  ss->orig_nodes_for_key = kh_init(m64vec);
  kret = kh_resize(m64vec, ss->orig_nodes_for_key, MAX_KEYS_PER_NODE);
  myassert(kret == 0);
  ss->idx_for_final_key = kh_init(m64);
  kret = kh_resize(m64, ss->idx_for_final_key, MAX_KEYS_PER_NODE);
  myassert(kret == 0);
  ss->req_keys_from_node = kh_init(m64vec);
  kret = kh_resize(m64vec, ss->req_keys_from_node, MAX_KEYS_PER_NODE);
  myassert(kret == 0);

  ss->vec_pool_head = &ss->vec_pool[0];
  for (int i = 0; i < VEC_POOL_SIZE-1; i++)
    ss->vec_pool[i].v[0] = (uint64_t)&ss->vec_pool[i+1];
  ss->vec_pool[VEC_POOL_SIZE-1].v[0] = (uint64_t)NULL;
}

void finalShuffle(struct sorter_state *ss) {
  unsigned recvd_item_reqs = 0, recvd_items = 0;
  //printf("[%d] recvd_keys_cnt=%d: ", ss->nid, ss->recvd_keys_cnt); printArr(ss->recvd_keys, ss->recvd_keys_cnt);

  int kret;
  khint_t kk;
  for (unsigned i = 0; i < ss->recvd_keys_cnt; i++) {
    kk = kh_put(m64, ss->idx_for_final_key, ss->recvd_keys[i], &kret);
    myassert(kret > -1);
    kh_value(ss->idx_for_final_key, kk) = i;
  }

  // Batch item requests for the same node
  for (unsigned key_idx = 0; key_idx < ss->recvd_keys_cnt; key_idx++) {
    const uint64_t key = ss->recvd_keys[key_idx];
    kk = kh_get(m64vec, ss->orig_nodes_for_key, key);
    myassert(kk != kh_end(ss->orig_nodes_for_key) && "I don't know who owns the key");
    struct simple_vector *v = kh_value(ss->orig_nodes_for_key, kk);
    const uint64_t orig_node_id = v->v[key_idx % v->size];

    kk = kh_get(m64vec, ss->req_keys_from_node, orig_node_id);
    if (kk == kh_end(ss->req_keys_from_node)) {
      kk = kh_put(m64vec, ss->req_keys_from_node, orig_node_id, &kret);
      myassert(kret > 0);
      kh_value(ss->req_keys_from_node, kk) = get_vec(ss);
    }
    v = kh_value(ss->req_keys_from_node, kk);
    myassert(v->size+1 < MAX_KEYS_PER_NODE);
    v->v[v->size++] = key;
  }

  // Send item requests
  uint64_t orig_node_id;
  struct simple_vector *v;
  kh_foreach(ss->req_keys_from_node, orig_node_id, v, {
    uint64_t orig_node_addr = g_dst_node_addrs[orig_node_id];
    unsigned key_cnt = v->size;
    myassert(key_cnt <= MAX_KEYS_PER_PKT);
    uint16_t msg_len = (2 + 1 + key_cnt)*8;
    uint64_t app_hdr = orig_node_addr | msg_len;
    lnic_write_r(app_hdr);
    lnic_write_i(0); // service_time
    lnic_write_i(0); // sent_time
    lnic_write_r(MSG_TYPE_SHUFL_REQ);
    for (unsigned i = 0; i < v->size; i++)
      lnic_write_m(v->v[i]);
  });

  // Check for any item reqs received during the previous step
  while (true) {
    unsigned key_cnt;
    uint64_t keys[MAX_KEYS_PER_PKT];
    uint64_t src_node_addr = 0;
    if (!recv_already_enqueued(MSG_TYPE_SHUFL_REQ, NULL, &key_cnt, keys, &src_node_addr, &ss->rb)) break;
    for (unsigned key_idx = 0; key_idx < key_cnt; key_idx++) {
      kk = kh_get(m64, ss->idx_for_orig_key, keys[key_idx]);
      myassert(kk != kh_end(ss->idx_for_orig_key) && "I don't own the requested key");
      send_item(src_node_addr, keys[key_idx], &ss->original_values[VALUE_SIZE_WORDS * kh_value(ss->idx_for_orig_key, kk)]);
    }
    recvd_item_reqs += key_cnt;
  }

  // In the same loop:
  //  a) receive and reply to item requests; and
  //  b) receive final items.
  while (recvd_item_reqs != GLOBAL_N || recvd_items != ss->recvd_keys_cnt) {
    lnic_wait();
    uint64_t app_hdr = lnic_read();
    uint16_t msg_len = (uint16_t)app_hdr;
    lnic_read(); // service_time
    lnic_read(); // sent_time
    uint64_t msg_type = lnic_read();
    if (msg_type == MSG_TYPE_SHUFL_REQ) { // item request
      uint64_t src_node_addr = app_hdr & (IP_MASK | CONTEXT_MASK);
      uint16_t key_cnt = ((msg_len/8) - (2 + 1));
      myassert(key_cnt <= MAX_KEYS_PER_PKT);
      //printf("[%d] (recvd_item_reqs=%d, recvd_items=%d) Got REQ: key_cnt=%d src_node_addr=0x%lx\n", ss->nid, recvd_item_reqs, recvd_items, key_cnt, src_node_addr);
      for (unsigned key_idx = 0; key_idx < key_cnt; key_idx++) {
        uint64_t key = lnic_read();
        kk = kh_get(m64, ss->idx_for_orig_key, key);
        myassert(kk != kh_end(ss->idx_for_orig_key) && "I don't own the requested key");
        send_item(src_node_addr, key, &ss->original_values[VALUE_SIZE_WORDS * kh_value(ss->idx_for_orig_key, kk)]);
      }
      myassert(recvd_item_reqs < GLOBAL_N);
      recvd_item_reqs += key_cnt;
    }
    else if (msg_type == MSG_TYPE_SHUFL_RES) { // item response
      myassert(recvd_items < ss->recvd_keys_cnt);
      if (msg_len != ITEM_MSG_SIZE_WORDS * 8) printf("[%d] (recv item) Error: expected msg_len to be %d but got %d\n", ss->nid, ITEM_MSG_SIZE_WORDS * 8, msg_len);
      recvd_items++;
      uint64_t key = lnic_read();
      //printf("[%d] (recvd_item_reqs=%d, recvd_items=%d) Got ITEM: key=%ld msg_len=%d\n", ss->nid, recvd_item_reqs, recvd_items, key, msg_len);
      kk = kh_get(m64, ss->idx_for_final_key, key);
      myassert(kk != kh_end(ss->idx_for_final_key));
      unsigned val_ofst = VALUE_SIZE_WORDS * kh_value(ss->idx_for_final_key, kk);

#if VALUE_SIZE_WORDS != 12
#error "This unrolled loop does not match VALUE_SIZE_WORDS"
#endif
      ss->final_values[val_ofst + 0] = lnic_read();
      ss->final_values[val_ofst + 1] = lnic_read();
      ss->final_values[val_ofst + 2] = lnic_read();
      ss->final_values[val_ofst + 3] = lnic_read();
      ss->final_values[val_ofst + 4] = lnic_read();
      ss->final_values[val_ofst + 5] = lnic_read();
      ss->final_values[val_ofst + 6] = lnic_read();
      ss->final_values[val_ofst + 7] = lnic_read();
      ss->final_values[val_ofst + 8] = lnic_read();
      ss->final_values[val_ofst + 9] = lnic_read();
      ss->final_values[val_ofst + 10] = lnic_read();
      ss->final_values[val_ofst + 11] = lnic_read();
    }
    else
      printf("Error: unexpected msg_type: %ld (msg_len=%d)\n", msg_type, msg_len);
    lnic_msg_done();
  }
}

void recSort(struct sorter_state *ss, unsigned rec_level) {
  uint64_t recvd_pivots[MAX_KEYS_PER_NODE];
  unsigned recvd_pivots_cnt = 0;
  uint64_t recvd_splitters[MAX_KEYS_PER_NODE];
  unsigned recvd_splitters_cnt = 0;
  int kret;
  khint_t kk;

  // Step 1a: arrange nodes into sets
  unsigned N = GLOBAL_N;
  unsigned num_groups = pow(N, rec_level);
  unsigned M = GLOBAL_M / num_groups;
  myassert((M%N) == 0 || M<N);
  unsigned columns, c;
  if (M > N) {
    c = M / N;
    columns = N;
  }
  else {
    c = 1;
    columns = M;
  }
  unsigned my_group_id = ss->nid / M;
  unsigned my_group_offset = my_group_id * M;
  unsigned my_offset_in_group = ss->nid - my_group_offset;
  unsigned my_i = my_offset_in_group / c;
  unsigned my_j = my_offset_in_group % c;
  unsigned num_medians = columns - 1;


#define DO_STEP2_OPTION2 1

  // Step 1a: send kth smallest key
  // If finding a single median, then I only participate if I'm in the first
  // group. If finding the median-of-medians (OPTION 2), we all participate.
  if (my_j == 0 || DO_STEP2_OPTION2) {
    for (unsigned i = 0; i < num_medians; i++) {
      // TODO: don't send to myself
      unsigned j = i * (ss->recvd_keys_cnt / (float)num_medians);
      unsigned dst_node = my_group_offset + (c*i) + my_j;
      if (!(dst_node < GLOBAL_M)) printf("[%d] M=%d N=%d c=%d i=%d my_group_id=%d my_i=%d my_j=%d dst_node=%d\n", ss->nid, M, N, c, i, my_group_id, my_i, my_j, dst_node);
      if (ss->recvd_keys_cnt > 0) send_key(MSG_TYPE_STEP1_KTH, dst_node, ss->recvd_keys[j]);
      else                   send_key_null(MSG_TYPE_STEP1_KTH, dst_node);
    }

    // This column will only receive medians if it's < the number of medians
    if (my_i < num_medians) {
      // Receive kth key (or null message) from each of the N nodes in the column
      // TODO: don't receive from myself
      for (unsigned recv_cnt = 0; recv_cnt < columns; recv_cnt++) {
        uint64_t med;
        uint8_t flags = recv_key(MSG_TYPE_STEP1_KTH, NULL, &med, NULL, &ss->rb);
        if ((flags & FLAG_NULL) == 0) // this is not a null message
          recvd_pivots[recvd_pivots_cnt++] = med;
      }
      //printf("[%d] recvd_pivots=%d: ", ss->nid, recvd_pivots_cnt); printArr(recvd_pivots, recvd_pivots_cnt);

      // Step 2a: Compute my median
      uint64_t median = ks_ksmall(uint64_t, recvd_pivots_cnt, recvd_pivots, recvd_pivots_cnt/2);

#if DO_STEP2_OPTION2
      unsigned K = MIN(c, (unsigned)10);
      if (my_j < K) { // If I'm one of the first K nodes in the column
        // Send my median to the first node of my column
        send_key(MSG_TYPE_STEP2, my_group_offset + (c*my_i), median);

        if (my_j == 0) { // If I'm the first node of the column
          recvd_pivots_cnt = 0;
          for (unsigned j = 0; j < K; j++) { // Receive the median from the first K nodes in my column
            recv_key(MSG_TYPE_STEP2, NULL, &recvd_pivots[recvd_pivots_cnt++], NULL, &ss->rb);
          }
          // Compute median of medians
          median = ks_ksmall(uint64_t, recvd_pivots_cnt, recvd_pivots, recvd_pivots_cnt/2);
        }
      }
#endif

      // Step 3: mcast medians
      if (my_j == 0) { // If I'm the first node of the column
        for (unsigned i = 0; i < M; i++) { // TODO: how to mcast here?
          unsigned dst_node = my_group_offset + i;
          send_key_and_src(MSG_TYPE_STEP3, dst_node, median, ss->nid);
        }
      }
    }
  }

  // Receive medians
  for (unsigned recv_cnt = 0; recv_cnt < num_medians; recv_cnt++) {
    uint64_t med, src_node;
    recv_key(MSG_TYPE_STEP3, NULL, &med, &src_node, &ss->rb);
    recvd_splitters[recvd_splitters_cnt++] = med;
  }

  // Sort the medians
  isort(recvd_splitters, recvd_splitters_cnt);

  //if (ss->nid == 0) { printf("[%d] rec=%d M=%d N=%d c=%d cols=%d splitters (%d):", ss->nid, rec_level, M, N, c, columns, recvd_splitters_cnt); printArr(recvd_splitters, recvd_splitters_cnt); }

  // Step 4a: send key x to bucket i s.t. m_{i-1} < x < m_i

  struct simple_vector keys_for_col[GLOBAL_N];
  for (unsigned i = 0; i < columns; i++) keys_for_col[i].size = 0;
  for (unsigned key_idx = 0; key_idx < ss->recvd_keys_cnt; key_idx++) {
    uint64_t key = ss->recvd_keys[key_idx];
    unsigned chosen_spl_idx = 0;
    for (unsigned spl_idx = 0; spl_idx < recvd_splitters_cnt; spl_idx++) {
      if (key < recvd_splitters[spl_idx]) break; // !(m_i <= x) == (x < m_i)
      chosen_spl_idx = spl_idx;
      if (num_medians < columns) chosen_spl_idx++;
    }
    //printf("[%d] key=%ld chosen_spl_idx=%d\n", ss->nid, key, chosen_spl_idx);
    myassert(keys_for_col[chosen_spl_idx].size+1 < MAX_KEYS_PER_NODE);
    keys_for_col[chosen_spl_idx].v[keys_for_col[chosen_spl_idx].size++] = key;
  }
  //printf("g_dst_node_addrs:"); for (unsigned i = 0; i < M; i++) printf(" %lx", g_dst_node_addrs[i]); printf("\n");
  //printf("[%d] recvd_splitters_cnt=%d\n", ss->nid, recvd_splitters_cnt);
  //printf("[%d] buckets dst_nodes: ", ss->nid); for (unsigned i = 0; i < recvd_splitters_cnt; i++) printf(" %d", node_for_splitter[recvd_splitters[i]]); printf("\n");

  // Send keys to each column
  for (unsigned i = 0; i < columns; i++) {
    unsigned dst_node = my_group_offset + (i*c) + my_j;
    myassert((dst_node % c) == my_j);
    myassert(dst_node < GLOBAL_M);
    //if (!((dst_node / c ) == i)) printf("[%d] M=%d N=%d c=%d i=%d dst_node=%d my_i=%d my_j=%d\n", ss->nid, M, N, c, i, dst_node, my_i, my_j);

    unsigned key_cnt = keys_for_col[i].size;
    // XXX key_cnt could be 0, in which case we send a "null message".
    uint16_t msg_len = (2 + 1 + 2*key_cnt) * 8;
    uint64_t app_hdr = g_dst_node_addrs[dst_node] | msg_len;
    lnic_write_r(app_hdr);
    lnic_write_i(0); // service_time
    lnic_write_i(0); // sent_time
    lnic_write_r(MSG_TYPE_STEP4);
    for (unsigned key_idx = 0; key_idx < key_cnt; key_idx++) {
      const uint64_t key = keys_for_col[i].v[key_idx];
      lnic_write_r(key);
      // XXX in case of duplicate keys, send a different origin node for each dup key:
      kk = kh_get(m64vec, ss->orig_nodes_for_key, key);
      myassert(kk != kh_end(ss->orig_nodes_for_key) && "I don't know who owns the key");
      struct simple_vector *v = kh_value(ss->orig_nodes_for_key, kk);
      const uint64_t orig_node_id = v->v[key_idx % v->size];
      lnic_write_r(orig_node_id);
    }
  }
  // I "lost" all these keys; no need to track their origin anymore:
  clear_nested_map(ss, ss->orig_nodes_for_key);

  // Receive my keys
  ss->recvd_keys_cnt = 0;
  for (unsigned recv_cnt = 0; recv_cnt < columns; recv_cnt++) {
    unsigned key_cnt;
    uint64_t src_nodes[MAX_KEYS_PER_PKT];
    recv_key(MSG_TYPE_STEP4, &key_cnt, &ss->recvd_keys[ss->recvd_keys_cnt], src_nodes, &ss->rb);
    for (unsigned i = 0; i < key_cnt; i++) {
      const uint64_t key = ss->recvd_keys[ss->recvd_keys_cnt + i];
      kk = kh_get(m64vec, ss->orig_nodes_for_key, key);
      if (kk == kh_end(ss->orig_nodes_for_key)) {
        kk = kh_put(m64vec, ss->orig_nodes_for_key, key, &kret);
        myassert(kret > 0);
        kh_value(ss->orig_nodes_for_key, kk) = get_vec(ss);
      }
      struct simple_vector *v = kh_value(ss->orig_nodes_for_key, kk);
      myassert(v->size+1 < MAX_KEYS_PER_NODE);
      v->v[v->size++] = src_nodes[i];
    }
    myassert(ss->recvd_keys_cnt + key_cnt <= MAX_KEYS_PER_NODE);
    ss->recvd_keys_cnt += key_cnt;
    //if (rec_level > 0) printf("[%d] recv_cnt=%u recvd_keys_cnt=%d\n", ss->nid, recv_cnt, ss->recvd_keys_cnt);
  }

  // Sort my keys
  isort(ss->recvd_keys, ss->recvd_keys_cnt);

  if (M > N)
    recSort(ss, rec_level+1);
  else
    finalShuffle(ss);
}

int run_node(int cid, uint64_t context_id, int nid) {
  struct sorter_state *ss = &g_ss[cid];
  //printf("[%d] Node started on context %lu.\n", nid, context_id);
  arch_spin_lock(&up_lock);
  if (cid < NCORES)
    init_core_state(ss, nid);
  arch_spin_unlock(&up_lock);

  send_ready_msg(cid, context_id);
  recv_start_msg();
  g_did_init = true;
  if (nid >= NNODES*NCORES) return EXIT_SUCCESS;

  //if (cid == 0) printf("[%d] Starting sort... g_total_alloc=%u\n", ss->nid, g_total_alloc);

  // Generate the keys for this node
  for (unsigned i = 0; i < GLOBAL_N; i++)
#if 0
    ss->original_keys[i] = nid*GLOBAL_N + i+1;
#else
    ss->original_keys[i] = rand();
#endif

  int kret;
  khint_t kk;
  for (unsigned i = 0; i < GLOBAL_N; i++) {
    kk = kh_put(m64, ss->idx_for_orig_key, ss->original_keys[i], &kret);
    if (!(kret > 0)) printf("[%d] got ret=%d key=%ld\n", ss->nid, kret, ss->original_keys[i]);
    myassert(kret > 0);
    kh_value(ss->idx_for_orig_key, kk) = i;
  }

  // Step 0: shuffle keys
  ks_shuffle(uint64_t, GLOBAL_N, ss->original_keys);
  //printf("[%d] original keys:", nid); printArr(ss->original_keys, GLOBAL_N);

  uint64_t sort_start = rdcycle();

  for (unsigned key_idx = 0; key_idx < GLOBAL_N; key_idx++) {
    uint64_t dst_node = g_shuffled_dst_node_ids[(ss->nid + key_idx) % GLOBAL_M];
    myassert(dst_node < GLOBAL_M);
    send_key_and_src(MSG_TYPE_STEP0, dst_node, ss->original_keys[key_idx], ss->nid);
  }

  // Receive exactly N keys
  for (unsigned recv_cnt = 0; recv_cnt < GLOBAL_N; recv_cnt++) {
    uint64_t orig_node;
    //orig_node = ss->nid; ss->recvd_keys[recv_cnt] = ss->original_keys[recv_cnt];
    recv_key(MSG_TYPE_STEP0, NULL, &ss->recvd_keys[recv_cnt], &orig_node, &ss->rb);
    const uint64_t *key = &ss->recvd_keys[recv_cnt];
    kk = kh_get(m64vec, ss->orig_nodes_for_key, *key);
    if (kk == kh_end(ss->orig_nodes_for_key)) {
      kk = kh_put(m64vec, ss->orig_nodes_for_key, *key, &kret);
      if (!(kret > 0)) printf("[%d] got ret=%d key=%ld orig_node=%ld\n", ss->nid, kret, *key, orig_node);
      myassert(kret > 0);
      kh_value(ss->orig_nodes_for_key, kk) = get_vec(ss);
    }
    struct simple_vector *v = kh_value(ss->orig_nodes_for_key, kk);
    myassert(v->size+1 < MAX_KEYS_PER_NODE);
    v->v[v->size++] = orig_node;
  }
  ss->recvd_keys_cnt = GLOBAL_N;
  isort(ss->recvd_keys, ss->recvd_keys_cnt);
  //printf("[%d] randomized keys:", ss->nid); printArr(ss->recvd_keys, ss->recvd_keys_cnt);

  recSort(ss, 0);

  uint64_t elapsed_sort = rdcycle() - sort_start;
  printf("[%d] keys=%d Elapsed: %ld\n", ss->nid, ss->recvd_keys_cnt, elapsed_sort);

  destroy_core_state(ss);

  ss->finished = true;

  if (cid == 0) {
    for (unsigned i = 0; i < NCORES; i++) while (!g_ss[i].finished) { }
    uint64_t prev_max = g_ss[0].recvd_keys_cnt < 1 ? 0 : maxArr(g_ss[0].recvd_keys, g_ss[0].recvd_keys_cnt);
    for (unsigned i = 1; i < NCORES; i++) {
      if (g_ss[i].recvd_keys_cnt == 0) continue;
      uint64_t this_max = maxArr(g_ss[i].recvd_keys, g_ss[i].recvd_keys_cnt),
               this_min = minArr(g_ss[i].recvd_keys, g_ss[i].recvd_keys_cnt);
      myassert(prev_max <= this_min);
      prev_max = this_max;
    }
    for (int i = 0; i < 40000; i++) asm volatile("nop"); // wait for other nodes to finish
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
  if (cid == 0) {
    for (unsigned i = 0; i < GLOBAL_M; i++) {
      g_shuffled_dst_node_ids[i] = i;
      g_dst_node_addrs[i] = (uint64_t)(BASE_NODE_IP + (i/4) ) << 32 | (i%4) << 16;
    }
    ks_shuffle(int, GLOBAL_M, g_shuffled_dst_node_ids);
    g_seed = nanopu_node_id;
    //for (unsigned i = 0; i < GLOBAL_M; i++) { printf(" %d=>0x%lx", g_shuffled_dst_node_ids[i], g_dst_node_addrs[g_shuffled_dst_node_ids[i]]); } printf("\n");
  }

  return run_node(cid, context_id, sorter_node_id);
}
