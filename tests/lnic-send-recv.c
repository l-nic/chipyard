#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "lnic.h"

#define NUM_MSG_WORDS 600

// Ring test
uint32_t get_dst_ip(uint32_t nic_ip_addr) {
    if (nic_ip_addr == 0x0a000005) {
        return 0x0a000002;
    } else if (nic_ip_addr < 0x0a000005 && nic_ip_addr >= 0x0a000002) {
        return nic_ip_addr + 1;
    } else {
        return 0;
    }
}

uint32_t get_correct_sender_ip(uint32_t nic_ip_addr) {
    if (nic_ip_addr == 0x0a000002) {
        return 0x0a000005;
    } else if (nic_ip_addr > 0x0a000002 && nic_ip_addr <= 0x0a000005) {
        return nic_ip_addr - 1;
    } else {
        return 0;
    }
}

// Pairwise loopback
// uint32_t get_dst_ip(uint32_t nic_ip_addr) {
//     if (nic_ip_addr == 0x0a000005) {
//         return 0x0a000003;
//     } else if (nic_ip_addr == 0x0a000003) {
//         return 0x0a000005;
//     } else if (nic_ip_addr == 0x0a000004) {
//         return 0x0a000002;
//     } else if (nic_ip_addr == 0x0a000002) {
//         return 0x0a000004;
//     } else {
//         return 0;
//     }
// }

// uint32_t get_correct_sender_ip(uint32_t nic_ip_addr) {
//     if (nic_ip_addr == 0x0a000005) {
//         return 0x0a000003;
//     } else if (nic_ip_addr == 0x0a000003) {
//         return 0x0a000005;
//     } else if (nic_ip_addr == 0x0a000004) {
//         return 0x0a000002;
//     } else if (nic_ip_addr == 0x0a000002) {
//         return 0x0a000004;
//     } else {
//         return 0;
//     }
// }

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

int main(int argc, char** argv)
{
    uint64_t app_hdr;
    uint64_t dst_ip;
    uint64_t dst_context;
    int num_words;
    int i;

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
    dst_ip = get_dst_ip(nic_ip_addr);
    if (dst_ip == 0) {
        printf("Could not find valid destination ip\n");
        return -1;
    }
    uint32_t correct_sender_ip = get_correct_sender_ip(nic_ip_addr);
    if (correct_sender_ip == 0) {
        printf("Could not find valid correct sender ip\n");
        return -1;
    }

    // register context ID with L-NIC
    lnic_add_context(0, 0);

for (int j = 0; j < 3; j++) {
    printf("Sending message\n");
    // Send the msg
    dst_context = 0;
    app_hdr = (dst_ip << 32) | (dst_context << 16) | (NUM_MSG_WORDS*8);
    //printf("Sending message\n");
    lnic_write_r(app_hdr);
    for (i = 0; i < NUM_MSG_WORDS; i++) {
        lnic_write_r(i);
    }
}

for (int k = 0; k < 3; k++) {
    printf("Receiving message\n");
    // Receive the msg
    lnic_wait();
    app_hdr = lnic_read();
    printf("Past wait with header %#lx\n", app_hdr);
    // Check dst IP
    uint64_t rx_dst_ip = (app_hdr & IP_MASK) >> 32;
    if (rx_dst_ip != correct_sender_ip) {
        printf("Expected: correct_sender_ip = %lx, Received: dst_ip = %lx\n", correct_sender_ip, rx_dst_ip);
        return -1;
    }
    // Check dst context
    uint64_t rx_dst_context = (app_hdr & CONTEXT_MASK) >> 16;
    if (rx_dst_context != dst_context) {
        printf("Expected: dst_context = %ld, Received: dst_context = %ld\n", dst_context, rx_dst_context);
        return -1;
    }
    uint16_t rx_msg_len = app_hdr & LEN_MASK;
    if (rx_msg_len != NUM_MSG_WORDS*8) {
        printf("Expected: msg_len = %d, Received: msg_len = %d\n", NUM_MSG_WORDS*8, rx_msg_len);
        return -1;
    }
    printf("Main receive loop\n");
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
    for (int i = 0; i < 100000; i++) {
        asm volatile("nop");
    }
    printf("Send recv program complete\n");  
    return 0;
}

