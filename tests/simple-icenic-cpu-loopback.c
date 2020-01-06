#include "mmio.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "icenic.h"
#include "encoding.h"

#define MAX_PKT_LEN 1500

int main(void)
{
  uint32_t pkt_buf[MAX_PKT_LEN/4];
  int pkt_len;
  int i;

  // receive pkt
  pkt_len = nic_recv(pkt_buf);
  printf("Received pkt of length: %d bytes\n", pkt_len);
  for (i = 0; i < pkt_len/sizeof(uint32_t); i++) {
    printf("%x\n", pkt_buf[i]);
  }
  // send pkt
  nic_send(pkt_buf, pkt_len);

  printf("Test Complete.\n");
  return 0;
}

