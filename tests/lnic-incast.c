#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "lnic.h"

#define NUM_MSG_WORDS 600
#define NUM_SENT_MESSAGES_PER_LEAF 3
#define FINISH_STALL_CYCLES 10000

#define NUM_LEAVES 3
uint32_t root_addr = 0x0a000002;
uint32_t leaf_addrs[NUM_LEAVES] = {0x0a000003, 0x0a000004, 0x0a000005};

bool valid_leaf_addr(uint32_t nic_ip_addr) {
    for (int i = 0; i < NUM_LEAVES; i++) {
        if (nic_ip_addr == leaf_addrs[i]) {
            return true;
        }
    }
    return false;
}

int prepare_printing(int argc, char** argv) {
    if (argc != 3) {
        printf("This program requires passing the L-NIC MAC address, followed by the L-NIC IP address.\n");
        return -1;
    }
    char* nic_mac_str = argv[1];
    if (strcmp(nic_mac_str, "0") != 0) {
        // Pass a zero for the MAC in simulation to disable this
        // printf("Program %s switching to UART printing...\n", argv[0]);
        // enable_uart_print(1);
    }
    printf("Total of %d arguments, which are (line-by-line):\n", argc);
    for (int i = 0; i < argc; i++) {
        printf("%s\n", argv[i]);
    }
    return 0;
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

    dst_context = 0;

    printf("Program starting...\n");
    
    if (prepare_printing(argc, argv) < 0) {
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

    // Register context ID with L-NIC
    lnic_add_context(0, 0);

    // Start the test
    if (nic_ip_addr == root_addr) {
        // This is the root node
        // Receive inbound messages from all leaves
        printf("Starting root node %#lx\n", nic_ip_addr);
        for (int i = 0; i < NUM_LEAVES*NUM_SENT_MESSAGES_PER_LEAF; i++) {
            printf("Receiving message %d of %d\n", i, NUM_LEAVES*NUM_SENT_MESSAGES_PER_LEAF);
            lnic_wait();
            app_hdr = lnic_read();
            printf("Past wait with header %#lx\n", app_hdr);
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
            printf("Entering main receive loop\n");
            // Check msg data
            for (i = 0; i < NUM_MSG_WORDS; i++) {
                uint64_t data = lnic_read();
                if (i != data) {
                    printf("Expected: data = %x, Received: data = %lx\n", i, data);
                    //return -1;
                } else if (i >= 0x1fe) {
                    // printf("got data %#lx\n", data);
                }
            }
            lnic_msg_done();
        }

        // Send one outbound message to each leaf node
        for (int i = 0; i < NUM_LEAVES; i++) {
            printf("Sending message\n");
            dst_context = 0;
            app_hdr = (leaf_addrs[i] << 32) | (dst_context << 16) | (NUM_MSG_WORDS*8);
            lnic_write_r(app_hdr);
            for (i = 0; i < NUM_MSG_WORDS; i++) {
                lnic_write_r(i);
            }
        }
        printf("Root program finished.\n");
        stall_cycles(FINISH_STALL_CYCLES);
        return 0;
    } else {
        if (!valid_leaf_addr(nic_ip_addr)) {
            printf("Supplied NIC IP is not a valid root or leaf address.\n");
            return -1;
        }
        // This is a valid leaf node
        // Send outbound messages
        printf("Starting leaf node %#lx\n", nic_ip_addr);
        for (int j = 0; j < NUM_SENT_MESSAGES_PER_LEAF; j++) {
            printf("Sending message\n");
            // Send the msg
            dst_context = 0;
            app_hdr = (root_addr << 32) | (dst_context << 16) | (NUM_MSG_WORDS*8);
            lnic_write_r(app_hdr);
            for (i = 0; i < NUM_MSG_WORDS; i++) {
                lnic_write_r(i);
            }
        }

        // Receive inbound message. (Only one for now)
        printf("Receiving message\n");
        lnic_wait();
        app_hdr = lnic_read();
        printf("Past wait with header %#lx\n", app_hdr);
        // Check src IP
        uint64_t rx_src_ip = (app_hdr & IP_MASK) >> 32;
        if (rx_src_ip != root_addr) {
            printf("Leaf node received message from non-root node at address %lx\n", rx_src_ip);
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
        printf("Entering main receive loop\n");
        // Check msg data
        for (i = 0; i < NUM_MSG_WORDS; i++) {
            uint64_t data = lnic_read();
            if (i != data) {
                printf("Expected: data = %x, Received: data = %lx\n", i, data);
                //return -1;
            } else if (i >= 0x1fe) {
                // printf("got data %#lx\n", data);
            }
        }
        lnic_msg_done();
        printf("Leaf program finished.\n");
        stall_cycles(FINISH_STALL_CYCLES);
        return 0;
    }
}

