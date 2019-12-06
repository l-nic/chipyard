#include <stdlib.h>
#include <stdio.h>
#include <string.h>

// LNIC CSRs
#define CSR_LMSGSRDY 0x052
#define CSR_LREAD 0x051
#define CSR_LWRITE 0x057
#define CSR_LWREND 0x058

#include "encoding.h"

#define PKT_LEN 8

int main(void)
{
	unsigned long data;
	int i;
	unsigned long start, end;

	start = rdcycle();
	// send pkt
	write_csr(0x057, 0);
	write_csr(0x057, 1);
	write_csr(0x057, 2);
	write_csr(0x057, 3);
	write_csr(0x057, 4);
	write_csr(0x057, 5);
	write_csr(0x057, 6);
	write_csr(0x058, 1); // write lwrend
	write_csr(0x057, 7);
	// receive pkt
	//while (read_csr(CSR_LMSGSRDY) == 0);
	while (read_csr(0x052) == 0);
	for (i = 0; i < PKT_LEN; i++) {
		//data = read_csr(CSR_LREAD);
		data = read_csr(0x051);
		if (data != i) {
			printf("Data mismatch: %lu != %d\n", data, i);
			exit(EXIT_FAILURE);
		}
	}
	end = rdcycle();
	
	// print latency
	printf("Test 0: send/recv %lu cycles\n", end - start);

	return 0;
}

