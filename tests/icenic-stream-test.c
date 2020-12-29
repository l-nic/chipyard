#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "encoding.h"
#include "mmio.h"
#include "icenic.h"

/**
 * Basic testing only version of IceNIC stream app.
 * Meant for checking simple latency and cache miss stats.
 * Use the batch version to test throughput.
 */

void inc_payload(uint64_t *words, int num_words) {
  int i;

  for (i = 0; i < num_words-1; i++) {
    words[i] = htonl(ntohl(words[i]) + 1);
  }
}

uint64_t buffer[ETH_MAX_WORDS];

int main(void)
{
  struct lnic_header *lnic;
  int len;
  uint64_t *data;
  int num_words;

  uint64_t start, dur;
  uint64_t start_misses, num_misses;

  uint64_t fake_buf[ETH_MAX_WORDS];

  // Setup mhpmcounter3 performance counter to count D$ misses
  write_csr(mhpmevent3, 0x202);

  printf("Ready!\n");

  while (1) {
    // receive pkt
    len = nic_recv_lnic(buffer, &lnic);
  
    // swap eth src and dst
    swap_eth(buffer);
  
    // increment every 8B word of the msg (except the last)
    num_words = ntohs(lnic->msg_len)/8;
    data = (void *)lnic + LNIC_HEADER_SIZE;

    start = rdcycle();
    start_misses = read_csr(mhpmcounter3);

    inc_payload(data, num_words);
//    inc_payload(fake_buf, num_words); 

    dur = rdcycle() - start;
    num_misses = read_csr(mhpmcounter3) - start_misses;

    // send pkt back out
    nic_send(buffer, len);
  
    printf("duration = %ld cycles\n", dur);
    printf("num misses = %ld\n", num_misses);
  }

  return 0;
}

