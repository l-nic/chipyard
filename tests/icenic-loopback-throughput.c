#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "mmio.h"
#include "icenic.h"

#define TSTAMP_BYTES 8

uint64_t buffer[ETH_MAX_WORDS];

int main(void)
{
  printf("Ready!\n");
  struct eth_header *eth;
  int len;
  uint8_t *buf_ptr;
  uint8_t start_time[TSTAMP_BYTES];

  // receive 1st pkt
  len = nic_recv(buffer);
  swap_eth(buffer);
  // extract start timestamp
  buf_ptr = buffer;
  memcpy(start_time, buf_ptr + len - TSTAMP_BYTES, TSTAMP_BYTES);
  // send pkt back out
  nic_send(buffer, len);

  // forward all subsequent pkts and copy over start timestamp
  while (1) {
    len = nic_recv(buffer);
    swap_eth(buffer);
    // copy over start timestamp
    buf_ptr = buffer;
    memcpy(buf_ptr + len - TSTAMP_BYTES, start_time, TSTAMP_BYTES);
    // send pkt back out
    nic_send(buffer, len);
  }

  return 0;
}

