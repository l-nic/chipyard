#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "lnic.h"

register volatile uint64_t global_ptr2 asm ("t6");


// We're going to need to lift most of the interrupt handling routines out of the linux
// source and put them here.
// The stvec will need to be routed to an exception handler.
// The sscratch register will need to be updated to keep a handle to our
// kernel data structures.
// Unlike in the actual kernel, everything is in supervisor (or machine)
// mode, so it can probably just stay there the whole time.

// We'll also need a little bit of the sbi interface, or we won't be
// able to set any timers or print anything from the kernel.

#define SBI_SET_TIMER 0
#define SBI_CONSOLE_PUTCHAR 1

#define REG_SP 2
#define REG_GP 3
#define REG_TP 4


#define SBI_CALL(which, arg0, arg1, arg2) ({      \
  register uintptr_t a0 asm ("a0") = (uintptr_t)(arg0); \
  register uintptr_t a1 asm ("a1") = (uintptr_t)(arg1); \
  register uintptr_t a2 asm ("a2") = (uintptr_t)(arg2); \
  register uintptr_t a7 asm ("a7") = (uintptr_t)(which);  \
  asm volatile ("ecall"         \
          : "+r" (a0)       \
          : "r" (a1), "r" (a2), "r" (a7)    \
          : "memory");        \
  a0;             \
})

#define SBI_CALL_1(which, arg0) SBI_CALL(which, arg0, 0, 0)


static inline void sbi_console_putchar(int ch)
{
  SBI_CALL_1(SBI_CONSOLE_PUTCHAR, ch);
}

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

static inline void sbi_set_timer(uint64_t stime_value)
{
  SBI_CALL_1(SBI_SET_TIMER, stime_value);
}

typedef unsigned long cycles_t;

static inline cycles_t get_cycles_inline(void)
{
  cycles_t n;

  __asm__ __volatile__ (
    "rdtime %0"
    : "=r" (n));
  return n;
}
#define get_cycles get_cycles_inline

static inline uint64_t get_cycles64(void)
{
        return get_cycles();
}


#define SIE_STIE 0x00000020 /* Timer Interrupt Enable */

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

struct thread_t {
  uintptr_t epc;
  uintptr_t regs[32];
  uint64_t priority;
  uintptr_t id;
  uint64_t skipped;
  int finished;
};

struct thread_t threads[10]; // TODO: This should really be a variable number
uint64_t num_threads = 0;

struct thread_t* new_thread() {
  csr_clear(mie, 0b10000000); // Don't want to accidentally hit a timer interrupt and loop through the threads right now
  if (num_threads == 10) {
    exit(22);
  }
  struct thread_t* next_thread = &threads[num_threads];
  next_thread->epc = 0;
  next_thread->priority = 1;
  next_thread->id = num_threads;
  next_thread->skipped = 0;
  next_thread->finished = 0;
  num_threads++;
  csr_set(mie, 0b10000000);
  return next_thread;
}

void remove_thread(struct thread_t* thread) {
  csr_clear(mie, 0b10000000); // Don't want to accidentally hit a timer interrupt and loop through the threads right now
  thread->finished = 1;
  csr_set(mie, 0b10000000);
}



uintptr_t handle_trap(uintptr_t cause, uintptr_t epc, uintptr_t* regs) {
  printf("cause is %#lx\n", cause);
  if (cause == 0x8000000000000007) {
    struct thread_t* current_thread = NULL;
    if (csr_read(mscratch) != 0) {
      // Back up the current user thread, if there is one
      current_thread = csr_read(mscratch);
      current_thread->epc = epc;
      for (int i = 0; i < 32; i++) {
        current_thread->regs[i] = regs[i];
      }
    }
    // Select a thread.to run
    struct thread_t* selected_thread = NULL;
    for (int i = 0; i < num_threads; i++) {
      struct thread_t* candidate_thread = &threads[i];
      if (candidate_thread->finished) {
        continue;
      }
      if (!selected_thread || candidate_thread->priority > selected_thread->priority || candidate_thread->skipped > selected_thread->skipped) {
        selected_thread = candidate_thread;
      }
    }

    // Increment the skipped count of every thread that was not chosen
    for (int i = 0; i < num_threads; i++) {
      struct thread_t* rejected_thread = &threads[i];
      if (rejected_thread == selected_thread || rejected_thread->priority < selected_thread->priority) {
        continue;
      }
      rejected_thread->skipped++;
    }
    if (!selected_thread) {
      exit(9);
    }

    // Switch to the new thread
    selected_thread->skipped = 0;
    printf("Switching to new thread %#lx at %#lx\n", selected_thread, selected_thread->epc);
    epc = selected_thread->epc;
    for (int i = 0; i < 32; i++) {
      regs[i] = selected_thread->regs[i];
    }
    csr_write(mscratch, selected_thread);

    // Restart the timer for the next timer interrupt
    uint64_t* mtime_ptr_lo = 0x200bff8;
    uint64_t* mtimecmp_ptr_lo = 0x2004000;
    *mtimecmp_ptr_lo = *mtime_ptr_lo + 100;

    return epc;
  }
  exit(99);
}

void start_thread(int (*target)(void)) {
  struct thread_t* thread = new_thread();
  thread->epc = target;
  volatile uint64_t sp, gp, tp;
  asm volatile("mv %0, sp" : "=r"(sp));
  asm volatile("mv %0, gp" : "=r"(gp));
  asm volatile("mv %0, tp" : "=r"(tp));
  thread->regs[REG_SP] = sp - 1024 * (1 + thread->id);
  thread->regs[REG_GP] = gp;
  thread->regs[REG_TP] = tp;


  //thread->regs[REG_SP] = 
}

int app1_main(void) {
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
//    printf("Received msg of length: %hu bytes", msg_len);
    num_words = msg_len/LNIC_WORD_SIZE;
    if (msg_len % LNIC_WORD_SIZE != 0) { num_words++; }
    // copy msg words back into network
    for (i = 0; i < num_words; i++) {
      lnic_copy();
    }
  }
  return 0;
}

int app2_main(void) {
  while (1) {
    printf("This is application 2\n");
  }
  return 0;
}

int main(void) {
  printf("Hello from scheduler base\n");

  // Turn on the timer
  uint64_t* mtime_ptr_lo = 0x200bff8;
  uint64_t* mtimecmp_ptr_lo = 0x2004000;
  *mtimecmp_ptr_lo = *mtime_ptr_lo + 100;
  csr_write(mscratch, 0);

  // Set up the application threads
  printf("Starting app 1\n");
  start_thread(app1_main);
  printf("Started app 1\n");
  start_thread(app2_main);
  printf("Started app 2\n");

  // Turn on the timer interrupts and wait for the scheduler to start
  csr_set(mie, 0b10000000);
  asm volatile ("wfi");
  
  // Should never reach here as long as user threads are running,
  // since the scheduler isn't aware that this thread exists.
  
  printf("Done.\n");
  return 0;
}

