#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "lnic.h"

#define NUM_MSG_WORDS 1024
#define NUM_SENT_MESSAGES_PER_LEAF 3

#define NUM_LEAVES 3
uint64_t root_addr = 0x0a000002;
uint64_t leaf_addrs[NUM_LEAVES] = {0x0a000003, 0x0a000004, 0x0a000005};

uint64_t elapsed_times[NUM_SENT_MESSAGES_PER_LEAF*NUM_LEAVES*2];

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

int main(int argc, char** argv)
{
    // Initialize variables and parse arguments
    uint64_t app_hdr;
    uint64_t dst_context;
    int i;
    volatile uint64_t current_time;

    dst_context = 0;

    if (argc != 3) {
        printf("This program requires passing the L-NIC MAC address, followed by the L-NIC IP address.\n");
        return -1;
    }

    char* nic_mac_str = argv[1];
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
    for (int j = 0; j < NUM_LEAVES*NUM_SENT_MESSAGES_PER_LEAF*2; j++) {
        volatile uint64_t x = elapsed_times[j] = 0;
    }

    // Register context ID with L-NIC
    lnic_add_context(0, 0);

    // Start the test
    if (nic_ip_addr == root_addr) {
        // This is the root node
        // Receive inbound messages from all leaves
        for (int j = 0; j < NUM_LEAVES*NUM_SENT_MESSAGES_PER_LEAF; j++) {
            lnic_wait();
            app_hdr = lnic_read();
            // Check src IP
            uint64_t rx_src_ip = (app_hdr & IP_MASK) >> 32;
            if (!valid_leaf_addr(rx_src_ip)) {
                printf("Root node received address from non-leaf node: %lx\n", rx_src_ip);
                return -1;
            }
            // Check src context
            uint64_t rx_src_context = (app_hdr & CONTEXT_MASK) >> 16;
            if (rx_src_context != dst_context) {
                printf("Expected: src_context = %ld, Received: rx_src_context = %ld\n", dst_context, rx_src_context);
                return -1;
            }
            // Check msg length
            uint16_t rx_msg_len = app_hdr & LEN_MASK;
            if (rx_msg_len != NUM_MSG_WORDS*8) {
                printf("Expected: msg_len = %d, Received: msg_len = %d\n", NUM_MSG_WORDS*8, rx_msg_len);
                return -1;
            }
            // Check msg data
            uint64_t sent_time = lnic_read();
            for (i = 0; i < NUM_MSG_WORDS - 1; i++) {
                uint64_t data = lnic_read();
                if (i != data) {
                    printf("Expected: data = %x, Received: data = %lx\n", i, data);
                    return -1;
                }
            }
            elapsed_times[2*j+1] = read_csr(mcycle) - sent_time;
            elapsed_times[2*j] = app_hdr;
            lnic_msg_done();
        }
        printf("&&CSV&&");
        for (int j = 0; j < NUM_LEAVES*NUM_SENT_MESSAGES_PER_LEAF*2; j++) {
            printf(",%lx", elapsed_times[j]);
        }
        printf("\n");
        printf("Root program finished.\n");

        // We need to be sure that all leaves have run to completion.
        stall_cycles(1000000);
        return 0;
    } else {
        if (!valid_leaf_addr(nic_ip_addr)) {
            printf("Supplied NIC IP is not a valid root or leaf address.\n");
            return -1;
        }
        // This is a valid leaf node
        // Send outbound messages
        for (int j = 0; j < NUM_SENT_MESSAGES_PER_LEAF; j++) {
            // Send the msg
            dst_context = 0;
            app_hdr = (root_addr << 32) | (dst_context << 16) | (NUM_MSG_WORDS*8);
            lnic_write_r(app_hdr);
            current_time = read_csr(mcycle);
            lnic_write_r(current_time);
            for (i = 0; i < NUM_MSG_WORDS - 1; i++) {
                lnic_write_r(i);
            }
        }
        printf("Leaf program finished.\n");
        
        // Only the root node is allowed to finish. The others will be killed by the simulation environment.
        while (1);
    }
}

