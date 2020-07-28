#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "lnic.h"
#include "lnic-scheduler.h"

// ------------------------------------------------------------
// Global information, used or read by all nodes and all cores
// ------------------------------------------------------------

// Same as the lock in syscalls.c
typedef struct {
  volatile unsigned int lock;
} arch_spinlock_t;

// Indicate to the nanokernel that this is a multicore program
bool is_single_core() { return false; }

// Number of 8-byte words in each message
#define NUM_MSG_WORDS 8

// Number of messages each leaf sends the root
#define NUM_SENT_MESSAGES_PER_LEAF 1

// Same as NCORES in syscalls.c
#define NCORES 4

// Number of services the root will provide
#define NSERVICES 2

// Size of the stats output buffer, in bytes
#define MAX_OUTPUT_LEN 1024

// Number of leaf nodes in the cluster. Total nodes will be (NUM_LEAVES + 1)
#define NUM_LEAVES 3

#define NUM_OUTPUT_FIELDS 3

// IP addr's are assigned by firesim starting at 10.0.0.2. Root will be the first one.
uint64_t root_addr = 0x0a000002;

// Leaf IP addr's are the following NUM_LEAVES addr's
uint64_t leaf_addrs[NUM_LEAVES] = {0x0a000003, 0x0a000004, 0x0a000005};

// ----------------------------------------------------------------
// Global information, read or written to by the root
// ----------------------------------------------------------------

// Total messages received across all cores and accompanying lock, per service
volatile uint32_t all_messages_received[NSERVICES];
arch_spinlock_t all_counter_locks[NSERVICES];

// Elapsed time information for each service
uint64_t all_elapsed_times[NSERVICES][NUM_SENT_MESSAGES_PER_LEAF*NUM_LEAVES*NUM_OUTPUT_FIELDS];

// Join mechanism for all root cores, for each service
bool all_roots_finished[NSERVICES][NCORES];

// Output buffer for service stats information
char output_buffer[NSERVICES][MAX_OUTPUT_LEN];

// -----------------------------------------------------------------
// Validation and stall utility functions
// -----------------------------------------------------------------

bool valid_leaf_addr(uint32_t nic_ip_addr) {
    for (int i = 0; i < NUM_LEAVES; i++) {
        if (nic_ip_addr == leaf_addrs[i]) {
            return true;
        }
    }
    return false;
}

void stall_cycles(uint64_t num_cycles) {
    for (uint64_t i = 0; i < num_cycles; i++) {
        asm volatile("nop");
    }
}

#define arch_spin_is_locked(x) ((x)->lock != 0)


static inline void arch_spin_unlock(arch_spinlock_t *lock) {
  asm volatile (
    "amoswap.w.rl x0, x0, %0"
    : "=A" (lock->lock)
    :: "memory"
    );
  set_csr(mie, LNIC_INT_ENABLE);
}

static inline int arch_spin_trylock(arch_spinlock_t* lock) {
  int tmp = 1, busy;
  asm volatile (
    "amoswap.w.aq %0, %2, %1"
    : "=r"(busy), "+A" (lock->lock)
    : "r"(tmp)
    : "memory"
    );
  return !busy;
}

static inline void arch_spin_lock(arch_spinlock_t* lock) {
  while (1) {
    if (arch_spin_is_locked(lock)) {
      continue;
    }
    clear_csr(mie, LNIC_INT_ENABLE);
    if (arch_spin_trylock(lock)) {
      break;
    }
    set_csr(mie, LNIC_INT_ENABLE);
  }
}

int root_node(uint64_t argc, char** argv, int cid, int nc, uint64_t context_id);

// --------------------------------------------------------------------------
// Application entry point. Run setup and then become a leaf or a root node.
// --------------------------------------------------------------------------

int core_main(int argc, char** argv, int cid) {
    // Initialize variables and parse arguments
    if (argc != 3) {
        printf("This program requires passing the L-NIC MAC address, followed by the L-NIC IP address.\n");
        return -1;
    }
    char* nic_ip_str = argv[2];
    uint32_t nic_ip_addr_lendian = 0;
    int retval = inet_pton4(nic_ip_str, nic_ip_str + strlen(nic_ip_str), &nic_ip_addr_lendian);

    // Risc-v is little-endian, but we store ip's as big-endian since the NIC works in big-endian
    uint32_t nic_ip_addr = swap32(nic_ip_addr_lendian);
    if (retval != 1 || nic_ip_addr == 0) {
        printf("Supplied NIC IP address is invalid.\n");
        return -1;
    }

    // Force elapsed_times to be brought into the cache
    for (int i = 0; i < NSERVICES; i++) {
        for (int j = 0; j < NUM_LEAVES*NUM_SENT_MESSAGES_PER_LEAF*NUM_OUTPUT_FIELDS; j++) {
            volatile uint64_t x = all_elapsed_times[i][j] = 0;
        }
    }

    // Start the test
    if (nic_ip_addr == root_addr) {
        // This is the root node
        scheduler_init();
        start_thread(root_node, 0, 1);
        start_thread(root_node, 1, 2);
        scheduler_run();
    } else {
        // Only use one core for leaves for now
        if (cid != 0) {
            while (1);
        }
        if (!valid_leaf_addr(nic_ip_addr)) {
            printf("Supplied NIC IP is not a valid root or leaf address.\n");
            return -1;
        }
        leaf_node();
        printf("Leaf program finished.\n");
        
        // Only the root node is allowed to finish. The others will be killed by the simulation environment.
        while (1);
    }
    printf("Incast program should never reach here.\n");
    return -1;
}

// ----------------------------------------------
// Node-specific root and leaf functions.
// ----------------------------------------------

void leaf_node() {
    // Set up leaf node data
    uint64_t dst_context, app_hdr, current_time;
    uint32_t i, j;
    dst_context = 0;

    // Register context ID with L-NIC
    lnic_add_context(0, 0);

    // Send outbound messages
    for (j = 0; j < NUM_SENT_MESSAGES_PER_LEAF; j++) {
        dst_context = 0;
        app_hdr = (root_addr << 32) | (dst_context << 16) | (NUM_MSG_WORDS*8);
        lnic_write_r(app_hdr);
        current_time = read_csr(mcycle);
        lnic_write_r(current_time);
        for (i = 0; i < NUM_MSG_WORDS - 1; i++) {
            lnic_write_r(i);
        }

        dst_context = 1;
        app_hdr = (root_addr << 32) | (dst_context << 16) | (NUM_MSG_WORDS*8);
        lnic_write_r(app_hdr);
        current_time = read_csr(mcycle);
        lnic_write_r(current_time);
        for (i = 0; i < NUM_MSG_WORDS - 1; i++) {
            lnic_write_r(i);
        }
    }
}

int root_node(uint64_t argc, char** argv, int cid, int nc, uint64_t context_id) {
    // Set up service-local and core- and service-local data
    arch_spinlock_t* counter_lock = &all_counter_locks[context_id];
    volatile uint32_t* messages_received = &all_messages_received[context_id];
    bool* root_finished = &all_roots_finished[context_id][cid];
    uint64_t* elapsed_times = all_elapsed_times[context_id];
    uint32_t local_messages_received, i, len_written;
    uint64_t app_hdr, rx_src_ip, rx_src_context, rx_msg_len, sent_time, data;
    bool all_finished, should_exit;
    should_exit = false;

    // Receive messages from leaf nodes
    while (1) {
        // Wait for either 1) A message to arrive or 2) The shared state to indicate all messages have been received
        while (1) {
            arch_spin_lock(counter_lock);
            if (*messages_received >= NUM_SENT_MESSAGES_PER_LEAF*NUM_LEAVES) {
                should_exit = true;
                break;
            } else if (lnic_ready()) {
                should_exit = false;
                break;
            } else {
                arch_spin_unlock(counter_lock);
                lnic_idle();
            }
        }
        if (should_exit) {
            arch_spin_unlock(counter_lock);
            break;
        }

        // Capture and update the shared state
        local_messages_received = *messages_received;
        (*messages_received)++;
        arch_spin_unlock(counter_lock);
        // Begin reading the message
        app_hdr = lnic_read();

        // Process the header
        // Check src IP
        rx_src_ip = (app_hdr & IP_MASK) >> 32;
        if (!valid_leaf_addr(rx_src_ip)) {
            printf("Root node received address from non-leaf node: %lx\n", rx_src_ip);
            return -1;
        }
        // Check src context
        rx_src_context = (app_hdr & CONTEXT_MASK) >> 16;
        if (rx_src_context != 0) {
            printf("Expected: src_context = %ld, Received: rx_src_context = %ld\n", 0, rx_src_context);
            return -1;
        }
        // Check msg length
        rx_msg_len = app_hdr & LEN_MASK;
        if (rx_msg_len != NUM_MSG_WORDS*8) {
            printf("Expected: msg_len = %d, Received: msg_len = %d\n", NUM_MSG_WORDS*8, rx_msg_len);
            return -1;
        }

        // Process the message data
        sent_time = lnic_read();
        elapsed_times[NUM_OUTPUT_FIELDS*local_messages_received+1] = read_csr(mcycle) - sent_time;
        for (i = 0; i < NUM_MSG_WORDS - 1; i++) {
            data = lnic_read();
            if (i != data) {
                //printf("Expected: data = %x, Received: data = %lx\n", i, data);
                //return -1;
            }
        }
        elapsed_times[NUM_OUTPUT_FIELDS*local_messages_received] = app_hdr;
        elapsed_times[NUM_OUTPUT_FIELDS*local_messages_received+2] = cid;
        lnic_msg_done();
    }

    // Indicate that this service on this core is finished
    *root_finished = true;

    // Wait for all other cores running this service.
    all_finished = false;
    while (!all_finished) {
        all_finished = true;
        for (i = 0; i < NCORES; i++) {
            if (!all_roots_finished[context_id][i]) {
                all_finished = false;
            }
        }
    }

    // TODO: We should really wait for all services to finish before printing anything

    // Only core 0 will handle stats for each service
    if (cid != 0) {
        return 0;
    }
    
    // Print the collected stats
    len_written = sprintf(output_buffer[context_id], "&&CSV&&,%d", context_id);
    for (i = 0; i < NUM_LEAVES*NUM_SENT_MESSAGES_PER_LEAF*NUM_OUTPUT_FIELDS; i++) {
        len_written += sprintf(output_buffer[context_id] + len_written, ",%lx", all_elapsed_times[context_id][i]);
    }
    len_written += sprintf(output_buffer[context_id] + len_written, "\n");

    printf("%s", output_buffer[context_id]);
    printf("Root program finished.\n");

    // We need to be sure that all leaves have run to completion.
    stall_cycles(10);
    return 0;
}
