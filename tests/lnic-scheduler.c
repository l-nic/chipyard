#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "lnic.h"
#include "lnic-scheduler.h"

// Kernel thread structure
struct thread_t {
  uintptr_t regs[30];
  uintptr_t epc;
} __attribute__((packed));

extern struct thread_t* threads;

// int app1_main(void) {
//   uint64_t app_hdr;
//   uint16_t msg_len;
//   int num_words;
//   int i;

//   // register context ID with L-NIC
//    //     lnic_add_context(0, 1);

//   while (1) {
//     // wait for a pkt to arrive
//     lnic_wait();
//     // read request application hdr
//     app_hdr = lnic_read();
//     // write response application hdr
//     lnic_write_r(app_hdr);
//     // extract msg_len
//     msg_len = (uint16_t)app_hdr;
// //    printf("Received msg of length: %hu bytes", msg_len);
//     num_words = msg_len/LNIC_WORD_SIZE;
//     if (msg_len % LNIC_WORD_SIZE != 0) { num_words++; }
//     // copy msg words back into network
//     for (i = 0; i < num_words; i++) {
//       lnic_add_int_i(1);
//     }
//   }
//   return 0;
// }

// int app2_main(void) {
//   //lnic_add_context(1, 0);
//   //while (1);
//     uint64_t app_hdr;
//   uint16_t msg_len;
//   int num_words;
//   int i;
//   while (1) {
//     // wait for a pkt to arrive
//     lnic_wait();
//     // read request application hdr
//     app_hdr = lnic_read();
//     // write response application hdr
//     lnic_write_r(app_hdr);
//     // extract msg_len
//     msg_len = (uint16_t)app_hdr;
// //    printf("Received msg of length: %hu bytes", msg_len);
//     num_words = msg_len/LNIC_WORD_SIZE;
//     if (msg_len % LNIC_WORD_SIZE != 0) { num_words++; }
//     // copy msg words back into network
//     for (i = 0; i < num_words; i++) {
//       lnic_add_int_i(2);
//     }
//   }
//   return 0;
// }

int app1_main(void) {
  while (1) {
    printf("App 1\n");
  }
  return 0;
}

int app2_main(void) {
  while (1) {
    printf("App 2\n");
  }
  return 0;
}

// Thread metadata storage in global state
uint64_t num_threads = 0;

void start_thread(int (*target)(void), uint64_t id, uint64_t priority) {
  struct thread_t* thread = threads + id;
  //printf("Thread addr is %#lx and struct size is %d\n", thread, sizeof(struct thread_t));
  lnic_add_context(id, priority);
  volatile uint64_t sp, gp, tp;
  asm volatile("mv %0, sp" : "=r"(sp));
  asm volatile("mv %0, gp" : "=r"(gp));
  asm volatile("mv %0, tp" : "=r"(tp));
  thread->regs[REG_SP] = sp - STACK_SIZE_BYTES * (1 + id);
  thread->regs[REG_GP] = gp;
  thread->regs[REG_TP] = tp;
  //thread->epc = target;
  for (int i = 0; i < 240; i += 8)
  *((char*)thread + 248*id + i) = target;
  printf("Thread is %#lx\n", thread);
}

int main(void) {
  //printf("Hello from scheduler base\n");

  // Turn on the timer
  uint64_t* mtime_ptr_lo = MTIME_PTR_LO;
  uint64_t* mtimecmp_ptr_lo = MTIMECMP_PTR_LO;
  *mtimecmp_ptr_lo = *mtime_ptr_lo + TIME_SLICE_RTC_TICKS;
  csr_write(mscratch, 0);

  // Set up the application threads
  //printf("Starting app 1\n");
  start_thread(app1_main, 0, 0);
  //printf("Started app 1\n");
  start_thread(app2_main, 1, 1);
  //printf("Started app 2\n");

  // Turn on the timer interrupts and wait for the scheduler to start
  csr_set(mie, TIMER_INT_ENABLE | LNIC_INT_ENABLE);
  asm volatile ("wfi");
  
  // Should never reach here as long as user threads are running,
  // since the scheduler isn't aware that this thread exists.
  
  printf("Done.\n");
  return 0;
}

