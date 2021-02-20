#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "encoding.h"
#include "mmio.h"
#include "icenic.h"
#include "dot-product.h"

/**
 * Basic testing only version of dot prod microbenchmark.
 * Meant only for basic latency and cache miss stats.
 * Use the batch version to test throughput.
 */

uint64_t compute_dot_prod(uint64_t *words, uint64_t *weights, int num_words) {
  uint64_t result = 0;
  int i;

  for (i = 0; i < num_words; i++) {
    uint64_t w = ntohl(words[i]);
    result += w * weights[w];
  }
  return result;
}

uint64_t buffer[ETH_MAX_WORDS];

int main(void)
{
  int len;
  int i;

  // headers
  struct lnic_header *lnic;
  struct dot_product_header *dot_prod;
  struct data_header *data;

  uint64_t start, dur;
  uint64_t start_misses, num_misses;

  // A "fake" buffer that will be L1 cache resident.
  // Use this to approximate NeBuLa runtime, which dispatches
  // message data into L1$.
  uint64_t fake_words[ETH_MAX_WORDS];
  for (i = 0; i < ETH_MAX_WORDS; i++) {
    fake_words[i] = i;
  }

  printf("Initializing...\n");
  // Initialize the weights
  uint64_t weights[NUM_WEIGHTS];
  for (i = 0; i < NUM_WEIGHTS; i++) {
    weights[i] = i;
  }
  // read weights to bring into cache
  for (i = 0; i < NUM_WEIGHTS; i++) {
    if (weights[i] != i) {
      printf("Failed to initialize weight: %d\n", i);
      return -1;
    }
  }

  // Setup mhpmcounter3 performance counter to count D$ misses
  write_csr(mhpmevent3, 0x202);

  printf("Ready!\n");
  while (1) {
    // receive pkt
    len = nic_recv_lnic(buffer, &lnic);

    start_misses = read_csr(mhpmcounter3);
    start = rdcycle();

    dot_prod = (void *)lnic + LNIC_HEADER_SIZE;
    if (ntohl(dot_prod->type) != DATA_TYPE) {
      printf("Expected Data msg.\n");
      return -1;
    }
    data = (void *)dot_prod + sizeof(struct dot_product_header);

    // Compute dot product
    uint64_t num_words = ntohl(data->num_words);
    uint64_t *words = (void *)data + sizeof(struct data_header);

    compute_dot_prod(words, weights, num_words);
//    compute_dot_prod(fake_words, weights, num_words);

    // swap eth src and dst
    swap_eth(buffer);

    // send pkt back out
    nic_send(buffer, len);

    dur = rdcycle() - start;
    num_misses = read_csr(mhpmcounter3) - start_misses;
  
    printf("duration = %ld cycles\n", dur);
    printf("num misses = %ld\n", num_misses);
  }

  return 0;
}

