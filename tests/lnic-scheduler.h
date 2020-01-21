
// Timer interrupt control
#define SIE_STIE 0x00000020 /* Timer Interrupt Enable */

// Stack setup calling convention register indices
#define REG_SP 2
#define REG_GP 3
#define REG_TP 4

// Kernel thread structure
struct thread_t {
  uintptr_t epc;
  uintptr_t regs[32];
  uint64_t priority;
  uintptr_t id;
  uint64_t skipped;
  int finished;
};

// CSR modification macros
#define csr_read(csr)           \
({                \
  register unsigned long __v;       \
  __asm__ __volatile__ ("csrr %0, " #csr      \
            : "=r" (__v) :      \
            : "memory");      \
  __v;              \
})

#define csr_write(csr, val)         \
({                \
  unsigned long __v = (unsigned long)(val);   \
  __asm__ __volatile__ ("csrw " #csr ", %0"   \
            : : "rK" (__v)      \
            : "memory");      \
})

#define csr_set(csr, val)         \
({                \
  unsigned long __v = (unsigned long)(val);   \
  __asm__ __volatile__ ("csrs " #csr ", %0"   \
            : : "rK" (__v)      \
            : "memory");      \
})

#define csr_clear(csr, val)         \
({                \
  unsigned long __v = (unsigned long)(val);   \
  __asm__ __volatile__ ("csrc " #csr ", %0"   \
            : : "rK" (__v)      \
            : "memory");      \
})
