#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "encoding.h"

// LNIC CSRs
#define CSR_LMSGSRDY 0x052

int main(void)
{
	unsigned long data;
	int i;
	unsigned long start, end;

	start = rdcycle();
	// send pkt
	asm ("li x31, 64"); // write msg len
	asm ("li x31, 1");
	asm ("li x31, 2");
	asm ("li x31, 3");
	asm ("li x31, 4");
	asm ("li x31, 5");
	asm ("li x31, 127");
	asm ("li x31, 128");
	asm ("li x31, 129");
	// receive pkt
	while (read_csr(0x052) == 0); // poll CSR_LMSGSRDY
	asm ("mv t0, x30"); // read msg len
	asm ("mv t0, x30");
	asm ("mv t0, x30");
	asm ("mv t0, x30");
	asm ("mv t0, x30");
	asm ("mv t0, x30");
	asm ("mv t0, x30");
	asm ("mv t0, x30");
	asm ("mv t0, x30");
	end = rdcycle();
	
	// print latency
	printf("Test 0: send/recv %lu cycles\n", end - start);

	return 0;
}

