#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "lnic.h"
#include "lnic-scheduler.h"


#define NCORES 2

int core_complete[NCORES];

void core0_app0() {
    while(true) {
        printf("core 0, app 0\n");
    }
}

void core0_app1() {
    while(true) {
        printf("core 0, app 1\n");
    }
}

void core1_app0() {
    while(true) {
        printf("core 1, app 0\n");
    }
}

void core1_app1() {
    while(true) {
        printf("core 1, app 1\n");
    }
}

// Kernel thread structure
struct thread_t {
    uintptr_t regs[NUM_REGS];
    uintptr_t epc;
    uintptr_t padding;
} __attribute__((packed));

// Thread metadata storage in global state
struct thread_t threads[NCORES][MAX_THREADS];
uint64_t num_threads[NCORES];

struct thread_t* new_thread() {
    csr_clear(mie, TIMER_INT_ENABLE);
    if (num_threads[csr_read(mhartid)] == MAX_THREADS) {
        exit(ERR_THREADS_EXHAUSTED);
    }
    struct thread_t* next_thread = &threads[csr_read(mhartid)][num_threads[csr_read(mhartid)]];
    next_thread->epc = 0;
    next_thread->padding = 0;
    num_threads[csr_read(mhartid)]++;
    csr_set(mie, TIMER_INT_ENABLE);
    return next_thread;
}

void start_thread(int (*target)(void), uint64_t id) {
    struct thread_t* thread = new_thread();
    thread->epc = target;
    volatile uint64_t sp, gp, tp;
    asm volatile("mv %0, sp" : "=r"(sp));
    asm volatile("mv %0, gp" : "=r"(gp));
    asm volatile("mv %0, tp" : "=r"(tp));
    thread->regs[REG_SP] = sp - STACK_SIZE_BYTES * (1 + id);
    thread->regs[REG_GP] = gp;
    thread->regs[REG_TP] = tp;
}

uintptr_t handle_trap(uintptr_t cause, uintptr_t epc, uintptr_t* regs) {

    // Verify valid interrupt
    printf("Cause is %#lx in hart %d\n", cause, csr_read(mhartid));
    uint64_t num_threads_this_core = num_threads[csr_read(mhartid)];
    if (cause != TIMER_INT_CAUSE) {
        exit(ERR_UNKNOWN_EXCEPTION);
    }

    //printf("Backing up at mscratch %#lx\n", csr_read(mscratch));
    // Back up the current thread, if there is one
    struct thread_t* current_thread = NULL;
    if (csr_read(mscratch) != 0) {
        current_thread = csr_read(mscratch);
        current_thread->epc = epc;
        for (int i = 0; i < NUM_REGS; i++) {
            current_thread->regs[i] = regs[i];
        }
    }

    //printf("Selecting thread from %d options\n", num_threads[csr_read(mhartid)]);
    // Select a thread to run
    struct thread_t* selected_thread = NULL;
    for (int i = 0; i < num_threads_this_core; i++) {
        struct thread_t* candidate_thread = &threads[csr_read(mhartid)][i];
        if (!selected_thread || candidate_thread->padding > selected_thread->padding) {
            selected_thread = candidate_thread;
        }
    }

    //printf("Incrementing skipped count of all threads but %#lx\n", selected_thread);

    // Increment the skipped count of every thread that was not chosen
    for (int i = 0; i < num_threads_this_core; i++) {
        struct thread_t* rejected_thread = &threads[csr_read(mhartid)][i];
        if (rejected_thread == selected_thread) {
            continue;
        }
        rejected_thread->padding++;
    }

    // Check that a thread was actually selected
    if (!selected_thread) {
        exit(ERR_NO_THREAD_SELECTED);
    }

    //printf("Switching to new thread at epc %#lx\n", selected_thread->epc);
    // Switch to the new thread
    selected_thread->padding = 0;
    epc = selected_thread->epc;
    for (int i = 0; i < NUM_REGS; i++) {
        regs[i] = selected_thread->regs[i];
    }
    csr_write(mscratch, selected_thread);

    // Restart the timer for the next timer interrupt
    uint64_t* mtime_ptr_lo = MTIME_PTR_LO;
    int hart_mtimecmp_ptr_lo = MTIMECMP_PTR_LO + csr_read(mhartid)*8;
    uint64_t* mtimecmp_ptr_lo = hart_mtimecmp_ptr_lo;
    *mtimecmp_ptr_lo = *mtime_ptr_lo + TIME_SLICE_RTC_TICKS;

    return epc;
}

void core0_main() {
    // Turn on the timer
    uint64_t* mtime_ptr_lo = MTIME_PTR_LO;
    uint64_t* mtimecmp_ptr_lo = MTIMECMP_PTR_LO;
    *mtimecmp_ptr_lo = *mtime_ptr_lo + TIME_SLICE_RTC_TICKS;
    csr_write(mscratch, 0);
    
    // Set up the application threads
    start_thread(core0_app0, 0);
    start_thread(core0_app1, 1);

    // Turn on the timer interrupts and wait for the scheduler to start
    csr_set(mie, TIMER_INT_ENABLE);
    asm volatile ("wfi");
      
    // Should never reach here as long as user threads are running,
    // since the scheduler isn't aware that this thread exists.
    printf("Done.\n");
}



void core1_main() {
    // Turn on the timer
    uint64_t* mtime_ptr_lo = MTIME_PTR_LO;
    uint64_t* mtimecmp_ptr_lo = MTIMECMP_PTR_LO + 8;
    *mtimecmp_ptr_lo = *mtime_ptr_lo + TIME_SLICE_RTC_TICKS;
    csr_write(mscratch, 0);

    // Set up the application threads
    start_thread(core1_app0, 0);
    start_thread(core1_app1, 1);

    // Turn on the timer interrupts and wait for the scheduler to start
    csr_set(mie, TIMER_INT_ENABLE);
    asm volatile ("wfi");
      
    // Should never reach here as long as user threads are running,
    // since the scheduler isn't aware that this thread exists.
    printf("Done.\n");
}

void notify_core_complete(int cid) {
    core_complete[cid] = true;
}

void wait_for_all_cores() {
    bool all_cores_complete = true;
    do {
        all_cores_complete = true;
        for (int i = 0; i < NCORES; i++) {
            if (!core_complete[i]) {
                all_cores_complete = false;
            }
        }
     } while (!all_cores_complete);
}

void thread_entry(int cid, int nc) {
    if (nc != 2 || NCORES != 2) {
        if (cid == 0) {
            printf("This program requires 2 cores but was given %d\n", nc);
        }
        return;
    }

    if (cid == 0) {
        for (int i = 0; i < NCORES; i++) {
            core_complete[i] = 0;
            num_threads[i] = 0;
        }
        core0_main();
        notify_core_complete(0);
        wait_for_all_cores();
        exit(0);
    } else {
        // cid == 1
        core1_main();
        notify_core_complete(1);
        wait_for_all_cores();
        exit(0);
    }
}

int main(void){

    // register context ID with L-NIC
        // lnic_add_context(0, 0);

//  while (1) {
//      // wait for a pkt to arrive
//      lnic_wait();
//      // read request application hdr
//      app_hdr = lnic_read();
//      // write response application hdr
//      lnic_write_r(app_hdr);
//      // extract msg_len
//      msg_len = (uint16_t)app_hdr;
// //       printf("Received msg of length: %hu bytes", msg_len);
//      num_words = msg_len/LNIC_WORD_SIZE;
//      if (msg_len % LNIC_WORD_SIZE != 0) { num_words++; }
//      // copy msg words back into network
//      for (i = 0; i < num_words; i++) {
//          lnic_copy();
//      }
//  }
    return 0;
}

