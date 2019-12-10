#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "encoding.h"

// LNIC CSRs
#define CSR_LMSGSRDY 0x052
#define CSR_LWREND 0x058

#define NUM_TESTS 4
#define PKT_LEN 8

int main(void)
{

	int count = 0;

  	while (count < NUM_TESTS) {
		// poll lmsgsrdy CSR until non-zero
		while (read_csr(0x052) == 0);
		// add one to head word and stuff into tail (x4)
		asm ("addi x31, x30, 1");
		asm ("addi x31, x30, 1");
		asm ("addi x31, x30, 1");
		write_csr(0x058, 1); // write lwrend
		asm ("addi x31, x30, 1");
		count += 1;
	}

	printf("Test Complete\n");
	return 0;
}

