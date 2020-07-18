#ifndef RISCV_LNIC_H
#define RISCV_LNIC_H

#include "encoding.h"

#define LNIC_WORD_SIZE 8

#define LREAD "x30"
#define LWRITE "x31"

#define IP_MASK      0xffffffff00000000
#define CONTEXT_MASK 0x00000000ffff0000
#define LEN_MASK     0x000000000000ffff

#define lnic_add_context(cid, priority) __extension__({ write_csr(0x053, cid); write_csr(0x055, priority); write_csr(0x054, 1); })

#define lnic_msg_done() write_csr(0x056, 1)

// poll CSR lmsgsrdy until non-zero
#define lnic_wait() while (read_csr(0x052) == 0) { write_csr(0x056, 2); }
#define lnic_ready() (read_csr(0x52) != 0)
#define lnic_idle() write_csr(0x56, 2)

#define lnic_read() ({ uint64_t __tmp; \
  asm volatile ("mv %0, " LREAD  : "=r"(__tmp)); \
  __tmp; })
#define lnic_copy() asm volatile ("mv " LWRITE ", " LREAD)
#define lnic_add_int_i(val)  asm volatile ("add " LWRITE ", " LREAD ", %0" : /*no outputs*/ : "i"(val))
#define lnic_write_r(val) asm volatile ("mv " LWRITE ", %0" : /*no outputs*/ : "r"(val))
#define lnic_write_i(val) asm volatile ("li " LWRITE ", %0" : /*no outputs*/ : "i"(val))
#define lnic_write_m(val) asm volatile ("ld " LWRITE ", %0" : /*no outputs*/ : "m"(val))

#define lnic_branch(inst, val, target) asm goto (inst" %0, " LREAD ", %1\n\t" : /*no outputs*/ : "r"(val) : /*no clobbers*/ : target)

#define lnic_boot() lnic_write_i(16); lnic_write_i(0); lnic_write_i(0)

#endif // RISCV_LNIC_H
