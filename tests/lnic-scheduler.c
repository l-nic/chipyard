#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "lnic.h"
#include "lnic-scheduler.h"

// Kernel thread structure
struct thread_t {
  uintptr_t regs[NUM_REGS];
  uintptr_t epc;
  uintptr_t padding;
} __attribute__((packed));

/**
 * High priority app - measure latency
 */
int app1_main(void) {
  uint64_t app_hdr;
  uint16_t msg_len;
  int num_words;
  int i;
  uint64_t stall_duration;

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
    stall_duration = lnic_read();
    for (i = 0; i < stall_duration; i++) {
      asm volatile("nop");
    }
    lnic_write_r(stall_duration);
    for (i = 1; i < num_words; i++) {
      lnic_copy();
    }
  }
  return 0;
}

/**
 * Low priority app - measure throughput
 */
int app2_main(void) {
  uint64_t app_hdr;
  uint16_t msg_len;
  int num_words;
  int i;
  uint64_t stall_duration;
  uint64_t start_time;
  while (1) {
    // wait for first msg to arrive
    lnic_wait();
    // read/write application hdr
    app_hdr = lnic_read();
    lnic_write_r(app_hdr);
    // extract msg_len
    msg_len = (uint16_t)app_hdr;
    num_words = msg_len/LNIC_WORD_SIZE;
    if (msg_len % LNIC_WORD_SIZE != 0) { num_words++; }
    // process data
    stall_duration = lnic_read();
    for (i = 0; i < stall_duration; i++) {
      asm volatile("nop");
    }
    lnic_write_r(stall_duration);
    // copy words back into network
    for (i = 2; i < num_words; i++) {
      lnic_copy();
    }
    // extract timestamp of first pkt
    start_time = lnic_read();
    lnic_write_r(start_time);
    // put start_time in all future msgs
    while (1) {
      lnic_wait();
      // read/write app_hdr
      app_hdr = lnic_read();
      lnic_write_r(app_hdr);
      // extract msg_len
      msg_len = (uint16_t)app_hdr;
      num_words = msg_len/LNIC_WORD_SIZE;
      if (msg_len % LNIC_WORD_SIZE != 0) { num_words++; }
      // process data
      stall_duration = lnic_read();
      for (i = 0; i < stall_duration; i++) {
        asm volatile("nop");
      }
      lnic_write_r(stall_duration);
      // copy words back into network
      for (i = 2; i < num_words; i++) {
        lnic_copy();
      }
      lnic_read(); // discard timestamp
      lnic_write_r(start_time);
    }
  }
  return 0;
}

// Thread metadata storage in global state
struct thread_t threads[MAX_THREADS + 2]; // Extra one for dummy main thread, and another because index 0 is reserved for asm scratch space
uint64_t num_threads = 1;

struct thread_t* new_thread() {
  csr_clear(mie, TIMER_INT_ENABLE);
  if (num_threads == MAX_THREADS) {
    exit(ERR_THREADS_EXHAUSTED);
  }
  struct thread_t* next_thread = &threads[num_threads];
  next_thread->epc = 0;
  num_threads++;
  csr_set(mie, TIMER_INT_ENABLE);
  return next_thread;
}

void start_thread(int (*target)(void), uint64_t id, uint64_t priority) {
  struct thread_t* thread = new_thread();
  thread->epc = target;
  lnic_add_context(id, priority);
  volatile uint64_t sp, gp, tp;
  asm volatile("mv %0, sp" : "=r"(sp));
  asm volatile("mv %0, gp" : "=r"(gp));
  asm volatile("mv %0, tp" : "=r"(tp));
  thread->regs[REG_SP] = sp - STACK_SIZE_BYTES * (1 + id);
  thread->regs[REG_GP] = gp;
  thread->regs[REG_TP] = tp;
}

int main(void) {
//  printf("Hello from scheduler base\n");

  // Turn on the timer
  uint64_t* mtime_ptr_lo = MTIME_PTR_LO;
  uint64_t* mtimecmp_ptr_lo = MTIMECMP_PTR_LO;
  *mtimecmp_ptr_lo = *mtime_ptr_lo + TIME_SLICE_RTC_TICKS;
  csr_write(mscratch, &threads[0]); // mscratch now holds thread base addr
  // Set up the application threads
//  printf("Starting app 1\n");
  start_thread(app1_main, 0, 0);
//  printf("Started app 1\n");
  start_thread(app2_main, 1, 1);
//  printf("Started app 2\n");

  // Turn on the timer interrupts and wait for the scheduler to start
  csr_write(0x53, num_threads - 1); // Set the main thread's id to an illegal value
  // This will keep it from being re-scheduled.
  // As long as it doesn't use the lnic, this should be fine.
  csr_set(mie, LNIC_INT_ENABLE);
  csr_clear(mie, TIMER_INT_ENABLE);
  asm volatile ("wfi");
  
  // Should never reach here as long as user threads are running,
  // since the scheduler isn't aware that this thread exists.
  
  printf("Done.\n"); 
  return 0;
}

