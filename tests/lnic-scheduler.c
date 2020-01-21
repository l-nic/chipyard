#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "lnic.h"
#include "lnic-scheduler.h"





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
  // printf("cause is %#lx\n", cause);
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
      csr_write(0x53, candidate_thread->id); // Set the current lnic context
      uint64_t candidate_thread_messages_pending = csr_read(0x52); // Read the pending messages
      uint64_t selected_thread_messages_pending = 0;
      if (selected_thread) {
        csr_write(0x53, selected_thread->id);
        selected_thread_messages_pending = csr_read(0x52);
      }
      if (!selected_thread || candidate_thread_messages_pending > selected_thread_messages_pending || candidate_thread->priority > selected_thread->priority || candidate_thread->skipped > selected_thread->skipped) {
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
    csr_write(0x53, selected_thread->id);

    // Restart the timer for the next timer interrupt
    uint64_t* mtime_ptr_lo = 0x200bff8;
    uint64_t* mtimecmp_ptr_lo = 0x2004000;
    *mtimecmp_ptr_lo = *mtime_ptr_lo + 40;

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

  // register context ID with L-NIC
        lnic_add_context(0, 0);

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
  uint64_t app_hdr;
  uint16_t msg_len;
  int num_words;
  int i;

  // register context ID with L-NIC
        lnic_add_context(1, 1);

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

