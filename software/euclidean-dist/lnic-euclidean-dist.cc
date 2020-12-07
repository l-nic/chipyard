#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include "lnic.h"

#define VECTOR_COUNT 10000
#define VECTOR_SIZE 64
#define VECTOR_SIZE_BYTES (VECTOR_SIZE * sizeof(uint16_t))
#define VECTOR_SIZE_WORDS (VECTOR_SIZE_BYTES / 8)

#include "euclidean-distance.c"

#define PRINT_TIMING 0
#define NCORES 1
#define USE_ONE_CONTEXT 1

// Expected address of the load generator
uint64_t load_gen_ip = 0x0a000001;

uint16_t vectors[VECTOR_COUNT][VECTOR_SIZE_WORDS];

void send_startup_msg(int cid, uint64_t context_id) {
  uint64_t app_hdr = (load_gen_ip << 32) | (0 << 16) | (2*8);
  uint64_t cid_to_send = cid;
  lnic_write_r(app_hdr);
  lnic_write_r(cid_to_send);
  lnic_write_r(context_id);
}

int run_server(int cid, uint64_t context_id) {
#if PRINT_TIMING
  uint64_t t0, t1, t2, t3;
#endif // PRINT_TIMING
  uint64_t app_hdr;
  uint64_t service_time, sent_time;
  uint64_t query_vector[VECTOR_SIZE_WORDS];

  printf("[%d] Server listenning on context_id=%ld.\n", cid, context_id);

  send_startup_msg(cid, context_id);

  while (1) {
    lnic_wait();
#if PRINT_TIMING
    t0 = rdcycle();
#endif // PRINT_TIMING
    app_hdr = lnic_read();
    //printf("[%d] --> Received msg of length: %u bytes\n", cid, (uint16_t)app_hdr);
    service_time = lnic_read();
    sent_time = lnic_read();
    for (unsigned i = 0; i < VECTOR_SIZE_WORDS; i++)
      query_vector[i] = lnic_read();

    uint64_t haystack_vector_cnt = lnic_read();
    uint64_t closest_vector_id = 0, closest_vector_dist = 0;

#if PRINT_TIMING
    t1 = rdcycle();
#endif // PRINT_TIMING

    for (unsigned i = 0; i < haystack_vector_cnt; i++) {
      uint64_t vector_id = lnic_read();
      if (vector_id > VECTOR_COUNT) printf("Invalid vector_id=%lu\n", vector_id);
      uint64_t dist = compute_dist_squared((uint16_t *)query_vector, vectors[vector_id-1]);
      if (closest_vector_id == 0 || dist < closest_vector_dist) {
        closest_vector_id = vector_id;
        closest_vector_dist = dist;
      }
    }
#if PRINT_TIMING
    t2 = rdcycle();
#endif // PRINT_TIMING

    uint64_t msg_len = 8 + 8 + 8;
    lnic_write_r((app_hdr & (IP_MASK | CONTEXT_MASK)) | msg_len);
    lnic_write_r(service_time);
    lnic_write_r(sent_time);
    lnic_write_r(closest_vector_id);

    lnic_msg_done();
#if PRINT_TIMING
    t3 = rdcycle();
    printf("[%d] haystack_vectors=%lu  closest=%lu  Load lat: %ld    Compute lat: %ld    Send lat: %ld     Total lat: %ld\n", cid,
        haystack_vector_cnt, closest_vector_id, t1-t0, t2-t1, t3-t2, t3-t0);
#endif // PRINT_TIMING
	}

  return EXIT_SUCCESS;
}

extern "C" {
bool is_single_core() { return false; }

int core_main(int argc, char** argv, int cid, int nc) {
  (void)argc; (void)argv; (void)nc;
  uint64_t priority = 0;
  uint64_t context_id = USE_ONE_CONTEXT ? 0 : cid;

  if (cid == 0) {
#if 1
    for (unsigned i = 0; i < VECTOR_COUNT; i++)
      for (unsigned j = 0; j < VECTOR_SIZE; j+=8) {
        vectors[i][j+0] = 1; vectors[i][j+1] = 1; vectors[i][j+2] = 1; vectors[i][j+3] = 1;
        vectors[i][j+4] = 1; vectors[i][j+5] = 1; vectors[i][j+6] = 1; vectors[i][j+7] = 1;
      }
    //memset(vectors, 1, sizeof(vectors));
#endif
  }

  if (cid > (NCORES-1)) {
    //lnic_add_context(context_id, priority);
    //send_startup_msg(cid, context_id);
    return EXIT_SUCCESS;
  }
  else {
    lnic_add_context(context_id, priority);
  }

  int ret = run_server(cid, context_id);
  return ret;
}
}
