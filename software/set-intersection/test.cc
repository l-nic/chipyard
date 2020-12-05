#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>

#include "intersection.h"

#define MAX_QUERY_WORDS 8
#define MAX_INSERSECTION_DOCS 128


#if 0
uint32_t *word_to_docids[3];
uint32_t word_to_docids_bin[] = {3, // number of word records
    2, // number of doc_ids
    1, // doc_id 1
    2, // doc_id 2
    3, 1, 3, 4,
    3, 1, 4, 5};
#else
#include "word_to_docids.h"
#endif

unsigned word_cnt;

int main(int argc, char **argv) {
  uint64_t query_word_cnt = 2;
  uint32_t query_word_ids[MAX_QUERY_WORDS];
  uint32_t *intersection_res, *intermediate_res;
  uint32_t intersection_tmp[2][1 + MAX_INSERSECTION_DOCS];

  query_word_ids[0] = 1;
  query_word_ids[1] = 2;

  if (argc > 1) {
    query_word_cnt = argc - 1;
    for (int i = 0; i < argc-1 && i < MAX_QUERY_WORDS; i++)
      query_word_ids[i] = (unsigned)atoi(argv[1+i]);
  }

  printf("query_word_ids (%d): ", query_word_cnt);
  for (unsigned i = 0; i < query_word_cnt; i++) printf("%d ", query_word_ids[i]);
  printf("\n");

  load_docs(&word_cnt, word_to_docids, word_to_docids_bin);
  printf("Loaded %d words.\n", word_cnt);

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

  printf("result: ");
  for (unsigned i = 0; i < intersection_res[0]; i++) {
    printf("%ld ", intersection_res[i+1]);
  }
  printf("\n");

  return 0;
}

