#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "lnic.h"

int main(void)
{

	// Register context IDs with L-NIC
        lnic_add_context(0, 0); // high priority context
        lnic_add_context(1, 1); // low priority context

	// If the following CSR bits are set then an L-NIC interrupt will fire
	// when a high priority msg arrives (i.e. a msg for context 0).
	// And the ltargetcontext CSR will be updated with context 0.
	// And mcause CSR will be updated with the interrupt exception code (16).
//	set_csr(mie, 0x10000); // enable L-NIC interrupts
//	set_csr(mstatus, 0b1000); // enable interrupts

	while (1);
	return 0;
}

