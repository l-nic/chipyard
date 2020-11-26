#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "mmio.h"
#include "icenic.h"

#define NUM_BUFS 50
#define TSTAMP_BYTES 8

uint64_t buffers[NUM_BUFS][ETH_MAX_WORDS];

/* Post a send request
 */
static void nic_post_send(void *buf, uint64_t len)
{
  uintptr_t addr = ((uintptr_t) buf) & ((1L << 48) - 1);
  unsigned long packet = (len << 48) | addr;
  while (nic_send_req_avail() == 0);
  reg_write64(SIMPLENIC_SEND_REQ, packet);
}

/* Wait for all send operations to complete
 */
static void nic_wait_send_batch(int tx_count)
{
  int i;

  for (i=0; i < tx_count; i++) {
    // Poll for completion
    while (nic_send_comp_avail() == 0);
    reg_read16(SIMPLENIC_SEND_COMP);
  }
}

/* Post a batch of RX requests
 */
static void nic_post_recv_batch(uint64_t bufs[][ETH_MAX_WORDS], int rx_count)
{
  int i;
  for (i=0; i < rx_count; i++) {
    uintptr_t addr = (uintptr_t) bufs[i];
    while (nic_recv_req_avail() == 0);
    reg_write64(SIMPLENIC_RECV_REQ, addr);
  }
}

/* Wait for a receive to complete
 */
static int nic_wait_recv()
{
  int len;

  // Poll for completion
  while (nic_recv_comp_avail() == 0);
  len = reg_read16(SIMPLENIC_RECV_COMP);
  asm volatile ("fence");

  return len;
}

int main(void)
{
  printf("Ready!\n");
  uint8_t start_time[TSTAMP_BYTES];
  int i;
  int len;

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

