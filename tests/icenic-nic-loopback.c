#include "mmio.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "icenic.h"
#include "encoding.h"

#define NUM_TESTS 10
#define PKT_LEN 16

int main(void)
{
	uint32_t src_pkt[PKT_LEN];
	uint32_t dst_pkt[PKT_LEN];
	int i, j;
	unsigned long start, end;
	
	// build pkt
	for (i = 0; i < PKT_LEN; i++) {
		src_pkt[i] = i;
	}

	for (j = 0; j < NUM_TESTS; j++) {
		start = rdcycle();
		// send pkt
		nic_send(src_pkt, PKT_LEN * sizeof(uint32_t));
		// receive pkt
		nic_recv(dst_pkt);	
		end = rdcycle();
	
		// check pkt
		for (i = 0; i < PKT_LEN; i++) {
			if (dst_pkt[i] != i) {
				printf("Data mismatch @ %d, %d: %x != %x\n", i, src_pkt[i], dst_pkt[i]);
				exit(EXIT_FAILURE);
			}
		}
		// print latency
		printf("Test %d: send/recv %lu cycles\n", j, end - start);
	}

	return 0;
}

