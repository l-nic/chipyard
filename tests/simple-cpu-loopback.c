#include "mmio.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "nic.h"
#include "encoding.h"

#define NUM_TESTS 10
#define PKT_LEN 8

int main(void)
{
        uint32_t pkt_buf[PKT_LEN];
        int i;
	int count = 0;

	while (count < NUM_TESTS) {
	        // receive pkt
	        nic_recv(pkt_buf);
	        // process pkt
	        for (i = 0; i < PKT_LEN; i++) {
	                pkt_buf[i] += 1;
	        }
	        // send pkt
	        nic_send(pkt_buf, PKT_LEN*sizeof(uint32_t));
		count += 1;
	}

	printf("Test Complete.\n");
        return 0;
}

