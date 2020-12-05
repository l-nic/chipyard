#include <stdio.h>
#include <stdlib.h>

#include "lnic.h"
#include "intersection.h"

#define MAX_QUERY_WORDS 8
#define MAX_INSERSECTION_DOCS 64

#define PRINT_TIMING 0
#define NCORES 1
#define USE_ONE_CONTEXT 1

// Expected address of the load generator
uint64_t load_gen_ip = 0x0a000001;

extern "C" {
    extern int _end;
    char* data_end = (char*)&_end + 16384*4;
    extern void sbrk_init(long int* init_addr);
    extern void __libc_init_array();
    extern void __libc_fini_array();
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

void send_startup_msg(int cid, uint64_t context_id) {
  uint64_t app_hdr = (load_gen_ip << 32) | (0 << 16) | (2*8);
  uint64_t cid_to_send = cid;
  lnic_write_r(app_hdr);
  lnic_write_r(cid_to_send);
  lnic_write_r(context_id);
}

int run_server(int cid, uint64_t context_id) {
	uint64_t app_hdr;
#if PRINT_TIMING
  uint64_t t0, t1, t2, t3;
#endif // PRINT_TIMING
  uint64_t service_time, sent_time;

  uint32_t query_word_ids[MAX_QUERY_WORDS];
  uint32_t *intersection_res, *intermediate_res;
  uint32_t intersection_tmp[2][1 + MAX_INSERSECTION_DOCS];

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
    uint64_t query_word_cnt = lnic_read();
    // TODO: store two word_ids per 8-byte word in the packet
    for (unsigned i = 0; i < query_word_cnt; i++) {
      query_word_ids[i] = lnic_read();
      if (query_word_ids[i] > word_cnt)
        printf("ERROR: received word_id > word_cnt (%u > %u)\n", query_word_ids[i], word_cnt);
    }
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

    unsigned intersection_size = intersection_res[0];
    uint64_t msg_len = 8 + 8 + 8 + (intersection_size * 8);
    lnic_write_r((app_hdr & (IP_MASK | CONTEXT_MASK)) | msg_len);
    lnic_write_r(service_time);
    lnic_write_r(sent_time);
    lnic_write_r(intersection_size);
    for (unsigned i = 0; i < intersection_size; i++)
      lnic_write_r(intersection_res[1+i]);

    lnic_msg_done();
#if PRINT_TIMING
    t3 = rdcycle();
    printf("[%d] query_words=%lu  res_docs=%u    Load lat: %ld    Compute lat: %ld    Send lat: %ld     Total lat: %ld\n", cid,
        query_word_cnt, intersection_size, t1-t0, t2-t1, t3-t2, t3-t0);
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
    // Setup the C++ libraries
    //sbrk_init((long int*)data_end);
    //atexit(__libc_fini_array);
    //__libc_init_array();

    load_docs(&word_cnt, word_to_docids, word_to_docids_bin);
    printf("Loaded %d words.\n", word_cnt);
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
