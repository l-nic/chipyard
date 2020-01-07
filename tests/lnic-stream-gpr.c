#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "lnic.h"

int main(void)
{
	uint64_t app_hdr;
	uint16_t msg_len;
	int num_words;
	int i;

	// send initial boot msg
	lnic_boot();

	while (1) {
		// wait for a pkt to arrive
		lnic_wait();
		// read request application hdr
		app_hdr = lnic_read();
		// write response application hdr
		lnic_write_r(app_hdr);
		// extract msg_len
		msg_len = (uint16_t)app_hdr;
//		printf("Received msg of length: %hu bytes", msg_len);
		num_words = msg_len/LNIC_WORD_SIZE;
		if (msg_len % LNIC_WORD_SIZE != 0) { num_words++; }
		// copy msg words back into network
		for (i = 0; i < num_words-1; i++) {
			asm volatile ("addi "LWRITE", "LREAD", 1\n\t");
		}
		lnic_copy(); // copy timestamp on last word
	}
	return 0;
}

