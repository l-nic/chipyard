#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "lnic.h"

#define NCORES 2

int core_complete[NCORES];

void core0_app0() {

}

void core0_app1() {

}

void core1_app0() {

}

void core1_app1() {

}

void core0_main() {
	int j = 1;
	for (int i = 0; i < 10000; i++) {
		j += 2;
	}
	printf("Hello from core 0 main at %d\n", j);
}

void core1_main() {
	for (int i = 0; i < 3; i++)
	printf("Hello from core 1 main at %d\n", i);
}

void notify_core_complete(int cid) {
	core_complete[cid] = true;
}

void wait_for_all_cores() {
	bool all_cores_complete = true;
	do {
		all_cores_complete = true;
		for (int i = 0; i < NCORES; i++) {
			if (!core_complete[i]) {
				all_cores_complete = false;
			}
		}
	 } while (!all_cores_complete);
}

void thread_entry(int cid, int nc) {
	if (nc != 2 || NCORES != 2) {
		if (cid == 0) {
			printf("This program requires 2 cores but was given %d\n", nc);
		}
		return;
	}

	if (cid == 0) {
		for (int i = 0; i < NCORES; i++) {
			core_complete[i] = 0;
		}
		core0_main();
		notify_core_complete(0);
		wait_for_all_cores();
		exit(0);
	} else {
		// cid == 1
		core1_main();
		notify_core_complete(1);
		wait_for_all_cores();
		exit(0);
	}
}

int main(void){

	// register context ID with L-NIC
        // lnic_add_context(0, 0);

// 	while (1) {
// 		// wait for a pkt to arrive
// 		lnic_wait();
// 		// read request application hdr
// 		app_hdr = lnic_read();
// 		// write response application hdr
// 		lnic_write_r(app_hdr);
// 		// extract msg_len
// 		msg_len = (uint16_t)app_hdr;
// //		printf("Received msg of length: %hu bytes", msg_len);
// 		num_words = msg_len/LNIC_WORD_SIZE;
// 		if (msg_len % LNIC_WORD_SIZE != 0) { num_words++; }
// 		// copy msg words back into network
// 		for (i = 0; i < num_words; i++) {
// 			lnic_copy();
// 		}
// 	}
	return 0;
}

