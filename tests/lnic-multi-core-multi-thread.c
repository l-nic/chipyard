#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "lnic.h"

#include "lnic-scheduler.h"

/*
Following are the available start hook function signatures. You should define only one
of them in any given program.

int main(void) -- single-core, no arguments
int main(int argc, char** argv) -- single-core with arguments
int core_main(int argc, char** argv, int cid, int nc) -- multi-core with arguments.
Each core will start at this function and pass in its own core ID to cid. nc will
contain the total number of cores, as defined in crt.S. In order to use core_main
instead of main, you also need to define bool is_single_core(void) to return false.

*/

bool is_single_core() {return false;}
#define NUM_MSG_WORDS 8
#define NUM_SENT_MESSAGES_PER_LEAF 3
#define NUM_LEAVES 3
#define NUM_ROOT_CONTEXTS 2
uint64_t root_addr = 0x0a000002;
uint64_t leaf_addrs[NUM_LEAVES] = {0x0a000003, 0x0a000004, 0x0a000005};

typedef struct {
  volatile unsigned int lock;
} arch_spinlock_t;
arch_spinlock_t ip_lock;
uint32_t nic_ip_addr;
bool ip_set;

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

int root(uint64_t argc, char** argv, int cid, int nc, uint64_t context_id, uint64_t priority);
int leaf();

int core_main(uint64_t argc, char** argv, int cid, int nc) {
    // Check for required arguments
    if (argc != 3) {
        printf("This program requires passing the L-NIC MAC address, followed by the L-NIC IP address.\n");
        return -1;
    }

    // Core 0 processes the arguments, other cores wait
    if (cid == 0) {
        printf("Total of %d arguments, which are (line-by-line):\n", argc);
        for (int i = 0; i < argc; i++) {
            printf("%s\n", argv[i]);
        }
        char* nic_mac_str = argv[1];
        char* nic_ip_str = argv[2];
        uint32_t nic_ip_addr_lendian = 0;
        int retval = inet_pton4(nic_ip_str, nic_ip_str + strlen(nic_ip_str), &nic_ip_addr_lendian);

        // Risc-v is little-endian, but we store ip's as big-endian since the NIC works in big-endian
        nic_ip_addr = swap32(nic_ip_addr_lendian);
        if (retval != 1 || nic_ip_addr == 0) {
            printf("Supplied NIC IP address is invalid.\n");
            nic_ip_addr = 0;
            arch_spin_lock(&ip_lock);
            ip_set = true;
            arch_spin_unlock(&ip_lock);
            return -1;
        }
        arch_spin_lock(&ip_lock);
        ip_set = true;
        arch_spin_unlock(&ip_lock);
    } else {
        while (1) {
            arch_spin_lock(&ip_lock);
            if (ip_set) {
                arch_spin_unlock(&ip_lock);
                break;
            } else {
                arch_spin_unlock(&ip_lock);
                stall_cycles(1000);
            }
        }
        if (nic_ip_addr == 0) {
            return -1;
        }
    }

    if (nic_ip_addr == root_addr) {
        // Start the root node's two services
        printf("[Core %d] Starting root node services...\n", cid);
        scheduler_init();
        start_thread(root, 0, 1);
        start_thread(root, 1, 2);
        scheduler_run();
        printf("[Core %d] Invalid control return to main function.\n", cid);
        return -1;
    } else {
        // Start the leaf nodes as bare-metal applications
        printf("[Core %d] Starting leaf application...\n", cid);
        int retval = leaf();
        return retval;
    }
}

int root(uint64_t argc, char** argv, int cid, int nc, uint64_t context_id, uint64_t priority) {
    // This is the root node

    // Declare stack variables
    uint64_t app_hdr, data;
    uint32_t rx_src_ip;
    uint16_t rx_src_context, rx_msg_len;
    uint32_t i, j;

    // Receive inbound messages from all leaves
    for (j = 0; j < NUM_LEAVES*NUM_SENT_MESSAGES_PER_LEAF; j++) {
        lnic_wait();
        app_hdr = lnic_read();
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
        // Check msg data
        for (i = 0; i < NUM_MSG_WORDS; i++) {
            data = lnic_read();
            if (i != data) {
                //printf("Expected: data = %x, Received: data = %lx\n", i, data);
                //return -1;
            }
        }
        lnic_msg_done();
        printf("[Core %d] finished message %d\n", cid, j);
    }

    // Send one outbound message to each leaf node
    for (j = 0; j < NUM_LEAVES; j++) {
        app_hdr = ((uint64_t)leaf_addrs[j] << 32) | (0 << 16) | (NUM_MSG_WORDS*8);
        lnic_write_r(app_hdr);
        for (i = 0; i < NUM_MSG_WORDS; i++) {
            lnic_write_r(i);
        }
    }
    return 0;
}

int leaf() {
    if (!valid_leaf_addr(nic_ip_addr)) {
        printf("Supplied NIC IP is not a valid root or leaf address.\n");
        return -1;
    }
    // This is a valid leaf node

    // Declare stack variables
    uint32_t i, j;
    uint64_t app_hdr, data;
    uint32_t rx_src_ip;
    uint16_t rx_src_context, rx_msg_len;

    // Add L-NIC context
    lnic_add_context(0, 0);
    // Send outbound messages
    for (j = 0; j < NUM_SENT_MESSAGES_PER_LEAF; j++) {
        // Send to root context 0
        app_hdr = (root_addr << 32) | (0 << 16) | (NUM_MSG_WORDS*8);
        lnic_write_r(app_hdr);
        for (i = 0; i < NUM_MSG_WORDS; i++) {
            lnic_write_r(i);
        }

        // // Send to root context 1
        app_hdr = (root_addr << 32) | (1 << 16) | (NUM_MSG_WORDS*8);
        lnic_write_r(app_hdr);
        for (i = 0; i < NUM_MSG_WORDS; i++) {
            lnic_write_r(i);
        }
    }

    // Receive inbound messages. (One from each context)
    for (j = 0; j < NUM_ROOT_CONTEXTS; j++) {
        lnic_wait();
        app_hdr = lnic_read();
        // Check src IP
        rx_src_ip = (app_hdr & IP_MASK) >> 32;
        if (rx_src_ip != root_addr) {
            printf("Leaf node received message from non-root node at address %lx\n", rx_src_ip);
            return -1;
        }
        // Check src context
        rx_src_context = (app_hdr & CONTEXT_MASK) >> 16;
        if (rx_src_context > 1) {
            printf("Expected: src_context 0 or 1, Received: rx_src_context = %ld\n", rx_src_context);
            return -1;
        }
        // Check msg length
        rx_msg_len = app_hdr & LEN_MASK;
        if (rx_msg_len != NUM_MSG_WORDS*8) {
            printf("Expected: msg_len = %d, Received: msg_len = %d\n", NUM_MSG_WORDS*8, rx_msg_len);
            return -1;
        }
        // Check msg data
        for (i = 0; i < NUM_MSG_WORDS; i++) {
            data = lnic_read();
            if (i != data) {
                printf("Expected: data = %x, Received: data = %lx\n", i, data);
                //return -1;
            }
        }
        lnic_msg_done();
    }
    return 0;
}
