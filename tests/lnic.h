#ifndef RISCV_LNIC_H
#define RISCV_LNIC_H

#include "encoding.h"

#define LNIC_WORD_SIZE 8

#define LREAD "x30"
#define LWRITE "x31"

// poll CSR lmsgsrdy until non-zero
#define lnic_wait() while (read_csr(0x052) == 0)
#define lnic_read() ({ uint64_t __tmp; \
  asm volatile ("mv %0, "LREAD  : "=r"(__tmp)); \
  __tmp; })
#define lnic_copy() asm volatile ("mv "LWRITE", "LREAD)
#define lnic_write_r(val) asm volatile ("mv "LWRITE", %0" : /*no outputs*/ : "r"(val))
#define lnic_write_i(val) asm volatile ("li "LWRITE", %0" : /*no outputs*/ : "i"(val))

#define lnic_boot() lnic_write_i(16); lnic_write_i(0); lnic_write_i(0)

#endif // RISCV_LNIC_H
