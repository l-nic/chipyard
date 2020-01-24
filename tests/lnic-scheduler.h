
#define MAX_THREADS 10
#define TIME_SLICE_RTC_TICKS 150

// Timer interrupt control
#define TIMER_INT_ENABLE 0b10000000 /* Timer interrupts enabled */
#define TIMER_INT_CAUSE  0x8000000000000007 /* Current interrupt is from timer */
#define MTIME_PTR_LO 0x200bff8
#define MTIMECMP_PTR_LO 0x2004000

// LNIC interrupt control
#define LNIC_INT_ENABLE 0x10000
#define LNIC_INT_CAUSE 0x8000000000000010

// Exit error codes
#define ERR_THREADS_EXHAUSTED 22
#define ERR_NO_THREAD_SELECTED 9
#define ERR_UNKNOWN_EXCEPTION 99

// Stack setup calling convention register indices
#define NUM_REGS 32
#define REG_SP 2
#define REG_GP 3
#define REG_TP 4
#define STACK_SIZE_BYTES 1024

// Kernel thread structure
struct thread_t {
  uintptr_t epc;
  uintptr_t regs[32];
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
