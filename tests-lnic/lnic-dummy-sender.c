#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "lnic.h"

#define MSG_LEN_PKTS 33

#define REPEAT_4(X) X X X X
#define REPEAT_8(X) REPEAT_4(X) REPEAT_4(X)
#define REPEAT_16(X) REPEAT_8(X) REPEAT_8(X)
#define REPEAT_32(X) REPEAT_16(X) REPEAT_16(X)
#define REPEAT_64(X) REPEAT_32(X) REPEAT_32(X)
#define REPEAT_128(X) REPEAT_64(X) REPEAT_64(X)

int main(int argc, char** argv)
{
  uint64_t app_hdr;
  int i; 
  uint64_t dst_ip = 0x0a000002;

  // register context ID with L-NIC
  uint64_t context_id = 0;
  uint64_t priority = 0;
  lnic_add_context(context_id, priority);

  char* nic_ip_str = argv[2];
  uint32_t nic_ip_addr_lendian = 0;
  int retval = inet_pton4(nic_ip_str, nic_ip_str + strlen(nic_ip_str), &nic_ip_addr_lendian);
  uint32_t nic_ip_addr = swap32(nic_ip_addr_lendian);

  printf("%x Ready!\n", nic_ip_addr);

  if (nic_ip_addr == 0x0a000003) {
    printf("Sending msg!\n");
    // send msg
    app_hdr = (dst_ip << 32) | (MSG_LEN_PKTS*1024);
    lnic_write_r(app_hdr);
    for (i = 0; i < MSG_LEN_PKTS; i++) {
      REPEAT_128(lnic_write_i(0);)
    }

    printf("Done!\n");
  }
  // // wait for a bit before exiting
  // for (i = 0; i < 500; i++) {
  //   asm volatile("nop");
  // }

  // spin for the rest of the simulation to allow others to run
  while(1);

  return 0;
}

