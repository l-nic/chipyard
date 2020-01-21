#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "lnic.h"
#include "lnic-scheduler.h"

int app1_main(void) {
  uint64_t app_hdr;
  uint16_t msg_len;
  int num_words;
  int i;

  // register context ID with L-NIC
   //     lnic_add_context(0, 1);

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
      lnic_add_int_i(1);
    }
  }
  return 0;
}

int app2_main(void) {
  //lnic_add_context(1, 0);
  //while (1);
    uint64_t app_hdr;
  uint16_t msg_len;
  int num_words;
  int i;
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
      lnic_add_int_i(2);
    }
  }
  return 0;
}

// Thread metadata storage in global state
struct thread_t threads[MAX_THREADS];
uint64_t num_threads = 0;

struct thread_t* new_thread() {
  csr_clear(mie, TIMER_INT_ENABLE);
  if (num_threads == MAX_THREADS) {
    exit(ERR_THREADS_EXHAUSTED);
  }
  struct thread_t* next_thread = &threads[num_threads];
  next_thread->epc = 0;
  next_thread->priority = 1;
  next_thread->id = num_threads;
  next_thread->skipped = 0;
  next_thread->finished = 0;
  num_threads++;
  csr_set(mie, TIMER_INT_ENABLE);
  return next_thread;
}

void remove_thread(struct thread_t* thread) {
  csr_clear(mie, TIMER_INT_ENABLE);
  thread->finished = 1;
  csr_set(mie, TIMER_INT_ENABLE);
}


uintptr_t handle_trap(uintptr_t cause, uintptr_t epc, uintptr_t* regs) {
  if (cause == TIMER_INT_CAUSE || cause == LNIC_INT_CAUSE) {
    // Back up the current user thread, if there is one
    struct thread_t* current_thread = NULL;
    if (csr_read(mscratch) != 0) {
      current_thread = csr_read(mscratch);
      current_thread->epc = epc;
      for (int i = 0; i < NUM_REGS; i++) {
        current_thread->regs[i] = regs[i];
      }
    }

    // Select a thread to run
    uint64_t target_context = csr_read(0x58);
    struct thread_t* selected_thread = NULL;
    for (int i = 0; i < num_threads; i++) {
      struct thread_t* candidate_thread = &threads[i];
      // Skip any threads that have completed
      if (candidate_thread->finished) {
        continue;
      }
      printf("Target context is %d\n", csr_read(0x58));

      // Automatically accept the NIC's suggested context, if it exists
      if (candidate_thread->id == target_context) {
        selected_thread = candidate_thread;
        break;
      }

      // Default to LRU if the NIC suggests a non-existent context
      if (!selected_thread || candidate_thread->skipped > selected_thread->skipped) {
        selected_thread = candidate_thread;
      }
    }

    // Increment the skipped count of every thread that was not chosen
    for (int i = 0; i < num_threads; i++) {
      struct thread_t* rejected_thread = &threads[i];
      if (rejected_thread == selected_thread) {
        continue;
      }
      rejected_thread->skipped++;
    }

    // Throw an error if no thread could be selected
    if (!selected_thread) {
      exit(ERR_NO_THREAD_SELECTED);
    }

    // Switch to the new thread
    selected_thread->skipped = 0;
    printf("Switching to new thread %d at %#lx\n", selected_thread->id, selected_thread->epc);
    epc = selected_thread->epc;
    for (int i = 0; i < NUM_REGS; i++) {
      regs[i] = selected_thread->regs[i];
    }
    csr_write(mscratch, selected_thread);
    csr_write(0x53, selected_thread->id);

    // Restart the timer for the next timer interrupt
    uint64_t* mtime_ptr_lo = MTIME_PTR_LO;
    uint64_t* mtimecmp_ptr_lo = MTIMECMP_PTR_LO;
    *mtimecmp_ptr_lo = *mtime_ptr_lo + TIME_SLICE_RTC_TICKS;

    return epc;
  }
  printf("Unknown exception with cause %#lx\n", cause);
  exit(ERR_UNKNOWN_EXCEPTION);
}

void start_thread(int (*target)(void), uint64_t id, uint64_t priority) {
  struct thread_t* thread = new_thread();
  thread->epc = target;
  thread->id = id;
  thread->priority = priority;
  lnic_add_context(id, priority);
  volatile uint64_t sp, gp, tp;
  asm volatile("mv %0, sp" : "=r"(sp));
  asm volatile("mv %0, gp" : "=r"(gp));
  asm volatile("mv %0, tp" : "=r"(tp));
  thread->regs[REG_SP] = sp - STACK_SIZE_BYTES * (1 + thread->id);
  thread->regs[REG_GP] = gp;
  thread->regs[REG_TP] = tp;
}

int main(void) {
  printf("Hello from scheduler base\n");

  // Turn on the timer
  uint64_t* mtime_ptr_lo = MTIME_PTR_LO;
  uint64_t* mtimecmp_ptr_lo = MTIMECMP_PTR_LO;
  *mtimecmp_ptr_lo = *mtime_ptr_lo + TIME_SLICE_RTC_TICKS;
  csr_write(mscratch, 0);

  // Set up the application threads
  printf("Starting app 1\n");
  start_thread(app1_main, 0, 0);
  printf("Started app 1\n");
  start_thread(app2_main, 1, 1);
  printf("Started app 2\n");

  // Turn on the timer interrupts and wait for the scheduler to start
  csr_set(mie, TIMER_INT_ENABLE | LNIC_INT_ENABLE);
  asm volatile ("wfi");
  
  // Should never reach here as long as user threads are running,
  // since the scheduler isn't aware that this thread exists.
  
  printf("Done.\n");
  return 0;
}

