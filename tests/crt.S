# See LICENSE for license details.

#include "encoding.h"
#include "lnic-scheduler.h"

#if __riscv_xlen == 64
# define LREG ld
# define SREG sd
# define REGBYTES 8
#else
# define LREG lw
# define SREG sw
# define REGBYTES 4
#endif

  .section ".text.init"
  .globl _start
_start:
  li  x1, 0
  li  x2, 0
  li  x3, 0
  li  x4, 0
  li  x5, 0
  li  x6, 0
  li  x7, 0
  li  x8, 0
  li  x9, 0
  li  x10,0
  li  x11,0
  li  x12,0
  li  x13,0
  li  x14,0
  li  x15,0
  li  x16,0
  li  x17,0
  li  x18,0
  li  x19,0
  li  x20,0
  li  x21,0
  li  x22,0
  li  x23,0
  li  x24,0
  li  x25,0
  li  x26,0
  li  x27,0
  li  x28,0
  li  x29,0
#  li  x30,0
#  li  x31,0

  # enable FPU and accelerator if present
  li t0, MSTATUS_FS | MSTATUS_XS | MSTATUS_MIE
  csrs mstatus, t0

  # make sure XLEN agrees with compilation choice
  li t0, 1
  slli t0, t0, 31
#if __riscv_xlen == 64
  bgez t0, 1f
#else
  bltz t0, 1f
#endif
2:
  li a0, 1
  sw a0, tohost, t0
  j 2b
1:

#ifdef __riscv_flen
  # initialize FPU if we have one
  la t0, 1f
  csrw mtvec, t0

  fssr    x0
  fmv.s.x f0, x0
  fmv.s.x f1, x0
  fmv.s.x f2, x0
  fmv.s.x f3, x0
  fmv.s.x f4, x0
  fmv.s.x f5, x0
  fmv.s.x f6, x0
  fmv.s.x f7, x0
  fmv.s.x f8, x0
  fmv.s.x f9, x0
  fmv.s.x f10,x0
  fmv.s.x f11,x0
  fmv.s.x f12,x0
  fmv.s.x f13,x0
  fmv.s.x f14,x0
  fmv.s.x f15,x0
  fmv.s.x f16,x0
  fmv.s.x f17,x0
  fmv.s.x f18,x0
  fmv.s.x f19,x0
  fmv.s.x f20,x0
  fmv.s.x f21,x0
  fmv.s.x f22,x0
  fmv.s.x f23,x0
  fmv.s.x f24,x0
  fmv.s.x f25,x0
  fmv.s.x f26,x0
  fmv.s.x f27,x0
  fmv.s.x f28,x0
  fmv.s.x f29,x0
  fmv.s.x f30,x0
  fmv.s.x f31,x0
1:
#endif

  # initialize trap vector
  la t0, trap_entry
  csrw mtvec, t0

  # initialize global pointer
.option push
.option norelax
  la gp, __global_pointer$
.option pop

  la  tp, _end + 63
  and tp, tp, -64

  # get core id
  csrr a0, mhartid
  # for now, assume only 1 core
  li a1, 1
1:bgeu a0, a1, 1b

  # give each core 128KB of stack + TLS
#define STKSHIFT 17
  sll a2, a0, STKSHIFT
  add tp, tp, a2
  add sp, a0, 1
  sll sp, sp, STKSHIFT
  add sp, sp, tp

  j _init

  .align 2
trap_entry:
  csrrw x2, mscratch, x2 # x2 now in mscratch
  beq x2, zero, non_thread # If no need to backup thread, skip ahead
  SREG x1, 1*REGBYTES(x2) # Store x1
  mv x1, x2 # New x1 will be the data structure reference
  csrr x2, mscratch # Put x2 back where it should be
  SREG x2, 2*REGBYTES(x1) # Store x2
  # x1 now contains the data structure ref, old x1 and x2 are pushed
  SREG x3, 3*REGBYTES(x1)
  SREG x4, 4*REGBYTES(x1)
  SREG x5, 5*REGBYTES(x1)
  SREG x6, 6*REGBYTES(x1)
  SREG x7, 7*REGBYTES(x1)
  SREG x8, 8*REGBYTES(x1)
  SREG x9, 9*REGBYTES(x1)
  SREG x10, 10*REGBYTES(x1)
  SREG x11, 11*REGBYTES(x1)
  SREG x12, 12*REGBYTES(x1)
  SREG x13, 13*REGBYTES(x1)
  SREG x14, 14*REGBYTES(x1)
  SREG x15, 15*REGBYTES(x1)
  SREG x16, 16*REGBYTES(x1)
  SREG x17, 17*REGBYTES(x1)
  SREG x18, 18*REGBYTES(x1)
  SREG x19, 19*REGBYTES(x1)
  SREG x20, 20*REGBYTES(x1)
  SREG x21, 21*REGBYTES(x1)
  SREG x22, 22*REGBYTES(x1)
  SREG x23, 23*REGBYTES(x1)
  SREG x24, 24*REGBYTES(x1)
  SREG x25, 25*REGBYTES(x1)
  SREG x26, 26*REGBYTES(x1)
  SREG x27, 27*REGBYTES(x1)
  SREG x28, 28*REGBYTES(x1)
  SREG x29, 29*REGBYTES(x1)
  csrr x4, mepc
  SREG x4, 30*REGBYTES(x1)
  j true_thread
non_thread:
  mv x1, x2
true_thread:
  # Now free to use all regs, x1 taken for structure ref
  csrr x2, mcause
  li x3, TIMER_INT_CAUSE
  beq x2, x3, known_int
  li x3, LNIC_INT_CAUSE
  beq x2, x3, known_int
  li a0, 77
  jal exit

known_int:
  # x1 has structure ref, x2 has mcause
  # All gpr's and mepc backed up
  csrr x3, 0x58 # Load target context id
  #la x4, threads # Load threads base address
  mv x4, zero
  beq x3, zero, context_zero
context_one:
  add x4, x4, 248 # Correct threads address
context_zero:
  # x1 initial structure ref, x4 final structure ref, x3 target context id

  # Restart the timer
  li x6, MTIME_PTR_LO
  LREG x5, 0(x6)
  addi x5, x5, TIME_SLICE_RTC_TICKS
  li x6, MTIMECMP_PTR_LO
  SREG x5, 0(x6)

  # Load new id's and return value
  csrw mscratch, x4
  csrw 0x53, x3
  LREG x6, 30*REGBYTES(x4)
  csrw mepc, x6

  # Remain in M-mode after mret
  li x7, MSTATUS_MPP
  csrs mstatus, x7

  mv x1, x4

  # x1 final structure ref
  # Restore all GPRs other than x1
  LREG x2, 2*REGBYTES(x1)
  LREG x3, 3*REGBYTES(x1)
  LREG x4, 4*REGBYTES(x1)
  LREG x5, 5*REGBYTES(x1)
  LREG x6, 6*REGBYTES(x1)
  LREG x7, 7*REGBYTES(x1)
  LREG x8, 8*REGBYTES(x1)
  LREG x9, 9*REGBYTES(x1)
  LREG x10, 10*REGBYTES(x1)
  LREG x11, 11*REGBYTES(x1)
  LREG x12, 12*REGBYTES(x1)
  LREG x13, 13*REGBYTES(x1)
  LREG x14, 14*REGBYTES(x1)
  LREG x15, 15*REGBYTES(x1)
  LREG x16, 16*REGBYTES(x1)
  LREG x17, 17*REGBYTES(x1)
  LREG x18, 18*REGBYTES(x1)
  LREG x19, 19*REGBYTES(x1)
  LREG x20, 20*REGBYTES(x1)
  LREG x21, 21*REGBYTES(x1)
  LREG x22, 22*REGBYTES(x1)
  LREG x23, 23*REGBYTES(x1)
  LREG x24, 24*REGBYTES(x1)
  LREG x25, 25*REGBYTES(x1)
  LREG x26, 26*REGBYTES(x1)
  LREG x27, 27*REGBYTES(x1)
  LREG x28, 28*REGBYTES(x1)
  LREG x29, 29*REGBYTES(x1)

  # Restore x1 and return
  LREG x1, 1*REGBYTES(x1)
  mret

.section ".data"
.globl threads
threads:
.skip REGBYTES*31*2

.section ".tdata.begin"
.globl _tdata_begin
_tdata_begin:

.section ".tdata.end"
.globl _tdata_end
_tdata_end:

.section ".tbss.end"
.globl _tbss_end
_tbss_end:

.section ".tohost","aw",@progbits
.align 6
.globl tohost
tohost: .dword 0
.align 6
.globl fromhost
fromhost: .dword 0
