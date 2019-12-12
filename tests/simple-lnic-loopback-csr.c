#include <stdlib.h>
#include <stdio.h>
#include <string.h>

// LNIC CSRs
#define CSR_LMSGSRDY 0x052
#define CSR_LREAD 0x051
#define CSR_LWRITE 0x057

#include "encoding.h"

#define PKT_LEN 8

int main(void)
{
	unsigned long data;
	unsigned long start, end;

	start = rdcycle();
	// send pkt
	write_csr(0x057, 64); // msg length
	write_csr(0x057, 1);
	write_csr(0x057, 2);
	write_csr(0x057, 3);
	write_csr(0x057, 4);
	write_csr(0x057, 5);
	write_csr(0x057, 6);
	write_csr(0x057, 7);
	write_csr(0x057, 8);
	// receive pkt
	while (read_csr(0x052) == 0); // poll lmsgsrdy
	asm ("csrr s2, 0x51"); // msg length
	asm ("csrr s3, 0x51");
	asm ("csrr s4, 0x51");
	asm ("csrr s5, 0x51");
	asm ("csrr s6, 0x51");
	asm ("csrr s7, 0x51");
	asm ("csrr s8, 0x51");
	asm ("csrr s9, 0x51");
	asm ("csrr s10, 0x51");
	
//	data = read_csr(0x051); // msg length
//	data = read_csr(0x051);
//	data = read_csr(0x051);
//	data = read_csr(0x051);
//	data = read_csr(0x051);
//	data = read_csr(0x051);
//	data = read_csr(0x051);
//	data = read_csr(0x051);
//	data = read_csr(0x051);
	end = rdcycle();
	
	// print latency
	printf("Test 0: send/recv %lu cycles\n", end - start);

	return 0;
}

