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
		asm ("csrr t0, 0x51"); // msg length
		asm ("csrw 0x57, t0");
		// add one to head word and stuff into tail (x8)
		// word 1
		asm ("csrr s1, 0x51");
		asm ("addi s1, s1, 1");
		asm ("csrw 0x57, s1");
		// word 2
		asm ("csrr s2, 0x51");
		asm ("addi s2, s2, 1");
		asm ("csrw 0x57, s2");
		// word 3
		asm ("csrr s3, 0x51");
		asm ("addi s3, s3, 1");
		asm ("csrw 0x57, s3");
		// word 4
		asm ("csrr s4, 0x51");
		asm ("addi s4, s4, 1");
		asm ("csrw 0x57, s4");
		// word 5
		asm ("csrr s5, 0x51");
		asm ("addi s5, s5, 1");
		asm ("csrw 0x57, s5");
		// word 6
		asm ("csrr s6, 0x51");
		asm ("addi s6, s6, 1");
		asm ("csrw 0x57, s6");
		// word 7
		asm ("csrr s7, 0x51");
		asm ("addi s7, s7, 1");
		asm ("csrw 0x57, s7");
		// word 8
		asm ("csrr s8, 0x51");
		asm ("addi s8, s8, 1");
		asm ("csrw 0x57, s8");
		count += 1;
	}

	printf("Test Complete\n");
	return 0;
}

