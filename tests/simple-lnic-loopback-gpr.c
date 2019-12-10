#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "encoding.h"

// LNIC CSRs
#define CSR_LMSGSRDY 0x052
#define CSR_LWREND 0x058

#define PKT_LEN 8

static inline void lnic_write(uint64_t word) {
	asm("mv x30, a0");
}

static inline uint64_t lnic_read() {
	register uint64_t to_return asm("a0");
	asm("mv a0, x31");
	return to_return;
}

int main(void)
{
	unsigned long data;
	int i;
	unsigned long start, end;

	start = rdcycle();
	// send pkt
	asm ("li x31, 0");
	asm ("li x31, 1");
	asm ("li x31, 2");
	asm ("li x31, 3");
	asm ("li x31, 4");
	asm ("li x31, 5");
	asm ("li x31, 6");
	write_csr(0x058, 1); // write lwrend
	asm ("li x31, 7");
	// receive pkt
	//while (read_csr(CSR_LMSGSRDY) == 0);
	while (read_csr(0x052) == 0);
	for (i = 0; i < PKT_LEN; i++) {
		asm ("mv t0, x30");
//		if (data != i) {
//			printf("Data mismatch: %lu != %d\n", data, i);
//			exit(EXIT_FAILURE);
//		}
	}
	end = rdcycle();
	
	// print latency
	printf("Test 0: send/recv %lu cycles\n", end - start);

	return 0;
}

