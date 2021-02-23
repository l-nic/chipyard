#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "encoding.h"
#include "mmio.h"
#include "icenic.h"

/**
 * Tasks:
 *   - receive a pkt
 *   - parse ethernet headers
 *   - swap eth src and dst
 *   - send pkt back out
 */

uint64_t buffer[ETH_MAX_WORDS];

int main(void)
{
  int len;

  printf("Ready!\n");
  nic_boot_pkt();
  while (1) {
    // receive pkt
    len = nic_recv(buffer);
    // swap eth headers
    swap_eth(buffer);
    // send pkt back out
    nic_send(buffer, len);
  }

  return 0;
}

