#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "mmio.h"
#include "icenic.h"

#define NUM_BUFS 10
#define TSTAMP_BYTES 8

uint64_t buffers[NUM_BUFS][ETH_MAX_WORDS];

static void nic_send_batch(uint64_t bufs[][ETH_MAX_WORDS], uint64_t *lens, int tx_count)
{
  int i;
  for (i=0; i < tx_count; i++) {
    uintptr_t addr = ((uintptr_t) bufs[i]) & ((1L << 48) - 1);
    unsigned long packet = (lens[i] << 48) | addr;
    while (nic_send_req_avail() == 0);
    reg_write64(SIMPLENIC_SEND_REQ, packet);
  }

  for (i=0; i < tx_count; i++) {
    // Poll for completion
    while (nic_send_comp_avail() == 0);
    reg_read16(SIMPLENIC_SEND_COMP);
  }
}

static void nic_recv_batch(uint64_t bufs[][ETH_MAX_WORDS], int rx_count, uint64_t *lens)
{
  int i;
  for (i=0; i < rx_count; i++) {
    uintptr_t addr = (uintptr_t) bufs[i];
    while (nic_recv_req_avail() == 0);
    reg_write64(SIMPLENIC_RECV_REQ, addr);
  }

  for (i=0; i < rx_count; i++) {
    // Poll for completion
    while (nic_recv_comp_avail() == 0);
    lens[i] = reg_read16(SIMPLENIC_RECV_COMP);
    asm volatile ("fence");
  }
}

int main(void)
{
  printf("Ready!\n");
  uint64_t lens[NUM_BUFS]; // store the lengths of each packet
  uint8_t *buf_ptr;
  uint8_t start_time[TSTAMP_BYTES];
  int i;

  // receive 1st batch of pkt
  nic_recv_batch(buffers, NUM_BUFS, lens);
  // extract timestamp from 1st pkt
  swap_eth(buffers[0]);
  buf_ptr = buffers[0];
  memcpy(start_time, buf_ptr + lens[0] - TSTAMP_BYTES, TSTAMP_BYTES);
  // copy timestamp into subsequent pkts
  for (i=1; i < NUM_BUFS; i++) {
    swap_eth(buffers[i]);
    buf_ptr = buffers[i];
    memcpy(buf_ptr + lens[i] - TSTAMP_BYTES, start_time, TSTAMP_BYTES);
  }
  // send batch of pkts back out 
  nic_send_batch(buffers, lens, NUM_BUFS);

  // forward all subsequent pkts and copy over start timestamp
  while (1) {
    nic_recv_batch(buffers, NUM_BUFS, lens);
    for (i=0; i < NUM_BUFS; i++) {
      swap_eth(buffers[i]);
      // copy over start timestamp
      buf_ptr = buffers[i];
      memcpy(buf_ptr + lens[i] - TSTAMP_BYTES, start_time, TSTAMP_BYTES);
    }
    // send pkt back out
    nic_send_batch(buffers, lens, NUM_BUFS);
  }

  return 0;
}

