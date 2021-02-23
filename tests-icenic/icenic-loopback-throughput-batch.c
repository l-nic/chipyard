#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "mmio.h"
#include "icenic.h"

#define NUM_BUFS 50
#define TSTAMP_BYTES 8

uint64_t buffers[NUM_BUFS][ETH_MAX_WORDS];

int main(void)
{
  uint8_t start_time[TSTAMP_BYTES];
  int i;
  int len;

  printf("Ready!\n");
  nic_boot_pkt();

  // receive 1st batch of pkt
  nic_post_recv_batch(buffers, NUM_BUFS);
  // process 1st pkt
  len = nic_wait_recv();
  // extract timestamp from 1st pkt
  swap_eth(buffers[0]);
  memcpy(start_time, (void *)buffers[0] + len - TSTAMP_BYTES, TSTAMP_BYTES);
  // send 1st pkt out
  nic_post_send(buffers[0], len);
  // copy timestamp into subsequent pkts
  for (i=1; i < NUM_BUFS; i++) {
    len = nic_wait_recv();
    swap_eth(buffers[i]);
    memcpy((void *)buffers[i] + len - TSTAMP_BYTES, start_time, TSTAMP_BYTES);
    nic_post_send(buffers[i], len);
  }
  // wait for all send operations to complete
  nic_wait_send_batch(NUM_BUFS);

  // forward all subsequent pkts and copy over start timestamp
  while (1) {
    nic_post_recv_batch(buffers, NUM_BUFS);
    for (i=0; i < NUM_BUFS; i++) {
      len = nic_wait_recv();
      swap_eth(buffers[i]);
      // copy over start timestamp
      memcpy((void *)buffers[i] + len - TSTAMP_BYTES, start_time, TSTAMP_BYTES);
      nic_post_send(buffers[i], len);
    }
    // wait for all send operations to complete
    nic_wait_send_batch(NUM_BUFS);
  }

  return 0;
}

