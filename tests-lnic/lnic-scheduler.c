#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "lnic.h"
#include "lnic-scheduler.h"

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
    lnic_msg_done();
  }
  return 0;
}

/**
 * Low priority app - measure latency
 */
int app2_main(void) {
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
    lnic_msg_done();
  }
  return 0;
}

extern uint64_t num_threads;
// Kernel thread structure
struct thread_t {
  uintptr_t regs[NUM_REGS];
  uintptr_t epc;
  uintptr_t padding;
} __attribute__((packed));
extern struct thread_t* threads;

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
  start_thread(app2_main, 1, 0);
//  printf("Started app 2\n");

  // Turn on the timer interrupts and wait for the scheduler to start
  csr_write(0x53, NANOKERNEL_CONTEXT); // Set the main thread's id to nanokernel context id
  csr_write(0x55, num_threads - 1); // Set the main thread's priority to a low value 
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

