#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "encoding.h"

// LNIC CSRs
#define CSR_LMSGSRDY 0x052

#define PKT_LEN 8

int main(void)
{
	unsigned long data;
	int i;
	unsigned long start, end;

	start = rdcycle();
	// send pkt
	asm ("li x31, 64"); // msg len
	asm ("li x31, 1");
	asm ("li x31, 2");
	asm ("li x31, 3");
	asm ("li x31, 4");
	asm ("li x31, 5");
	asm ("li x31, 6");
	asm ("li x31, 7");
	asm ("li x31, 8");
	// receive pkt
	while (read_csr(0x052) == 0); // poll CSR_LMSGSRDY
	for (i = 0; i < PKT_LEN+1; i++) { // 8 words + msg_len word
		asm ("mv t0, x30");
	}
	end = rdcycle();
	
	// print latency
	printf("Test 0: send/recv %lu cycles\n", end - start);

	return 0;
}

