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
  # for now, assume only 4 cores
  li a1, 4
1:bgeu a0, a1, 1b

  bne zero, a0, 3f

  la t0, __bss_start
  la t1, __bss_end
  bgeu t0, t1, 3f
2: SREG zero, (t0)
   addi t0, t0, REGBYTES
   bltu t0, t1, 2b
3:

  # give each core 128KB of stack + TLS, changed from 17 to 30 leftshift
#define STKSHIFT 17
  sll a2, a0, STKSHIFT
  add tp, tp, a2
  add sp, a0, 1
  sll sp, sp, STKSHIFT
  add sp, sp, tp

  # Do not do this when using IceNIC!
  # start timer on NIC to track msg processing time
  csrwi 0x54, 0x4

  j _init

  .align 2
trap_entry:
  addi sp, sp, -272

  # Calculate the base and current thread struct addr
  csrrw x2, mscratch, x2 # Store x2 so we have a scratch reg
  SREG x1, 1*REGBYTES(x2) # Store x1 so we have a second scratch reg
  csrr x1, 0x53 # Load current context id
  addi x1, x1, 0x1 # Adjust for one-offset in threads
  slli x1, x1, 0x8 # Account for thread struct size
  add x1, x2, x1 # Current thread struct addr now in x1, base addr now in x2

  # Save the registers that were moved around earlier
  SREG x3, 3*REGBYTES(x1)
  csrr x3, mscratch # Load the old x2 value
  SREG x3, 2*REGBYTES(x1)
  LREG x3, 1*REGBYTES(x2) # Load the old x1 value
  SREG x3, 1*REGBYTES(x1)
  csrw mscratch, x2 # Save the base addr back in mscratch
  csrr x3, mepc
  SREG x3, 30*REGBYTES(x1)

  # All registers from x4 to x29 can now be stored normally
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

  # Handle exceptions other than scheduling interrupts
  csrr x3, mcause
  li x4, TIMER_INT_CAUSE
  beq x3, x4, valid
  li x4, LNIC_INT_CAUSE
  beq x3, x4, valid
  mv a0, x3
  j exit

valid:
  # restart timer on NIC to track msg processing time
  csrwi 0x54, 0x4
  csrr x4, 0x59 # Read target priority
  csrr x1, 0x58 # Read target context id
  csrw 0x53, x1 # Update current context id # TODO: Do we still need a pipeline stall after this?
  csrw 0x55, x4 # Update current priority
  addi x1, x1, 0x1 # Adjust for one-offset in threads
  slli x1, x1, 0x8 # Account for thread struct size
  add x1, x2, x1 # Target context now in x1, base addr in x2

  # Load target context x4 to x29
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

  # Reset the timer interrupt
  li x2, MTIMECMP_PTR_LO
  csrr x3, mhartid # Account for timer offset due to hartid
  slli x3, x3, 3
  add x2, x2, x3
  li x3, MTIME_PTR_LO
  LREG x3, 0(x3)
  addi x3, x3, TIME_SLICE_RTC_TICKS
  SREG x3, 0(x2)
  

  # Load target context remaining regs
  LREG x3, 30*REGBYTES(x1)
  csrw mepc, x3 # Load mepc
  li x3, MSTATUS_MPP
  csrs mstatus, x3 # Hold in M-mode
  LREG x3, 3*REGBYTES(x1) # Load x3
  LREG x2, 2*REGBYTES(x1)
  LREG x1, 1*REGBYTES(x1)

  addi sp, sp, 272
  mret

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
