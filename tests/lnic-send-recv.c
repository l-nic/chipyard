#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "lnic.h"
#include "mmio.h"

#define UART_BASE_ADDR 0x54000000
#define UART_TX_FIFO   0x00
#define UART_TX_CTRL   0x08
#define UART_TX_EN     0x01
#define UART_RX_CTRL   0x0c
#define UART_DIV       0x18


#define NUM_MSG_WORDS 10

uint32_t get_dst_ip(uint32_t nic_ip_addr) {
    if (nic_ip_addr == 0x0a000002) {
        return 0x0a000003;
    } else if (nic_ip_addr == 0x0a000003) {
        return 0x0a000004;
    } else if (nic_ip_addr == 0x0a000004) {
        return 0x0a000005;
    } else if (nic_ip_addr == 0x0a000005) {
        return 0x0a000006;
    } else if (nic_ip_addr == 0x0a000006) {
        return 0x0a000007;
    } else if (nic_ip_addr == 0x0a000007) {
        return 0x0a000008;
    } else if (nic_ip_addr == 0x0a000008) {
        return 0x0a000009;
    } else if (nic_ip_addr == 0x0a000009) {
        return 0x0a00000a;
    } else if (nic_ip_addr == 0x0a00000a) {
        return 0x0a00000b;
    } else if (nic_ip_addr == 0x0a00000b) {
        return 0x0a00000c;
    } else if (nic_ip_addr == 0x0a00000c) {
        return 0x0a00000d;
    } else if (nic_ip_addr == 0x0a00000d) {
        return 0x0a00000e;
    } else if (nic_ip_addr == 0x0a00000e) {
        return 0x0a00000f;
    } else if (nic_ip_addr == 0x0a00000f) {
        return 0x0a000010;
    } else if (nic_ip_addr == 0x0a000010) {
        return 0x0a000011;
    } else if (nic_ip_addr == 0x0a000011) {
        return 0x0a000002;
    } else {
        return 0;
    }
}

uint32_t get_correct_sender_ip(uint32_t nic_ip_addr) {
    if (nic_ip_addr == 0x0a000002) {
        return 0x0a000011;
    } else if (nic_ip_addr == 0x0a000003) {
        return 0x0a000002;
    } else if (nic_ip_addr == 0x0a000004) {
        return 0x0a000003;
    } else if (nic_ip_addr == 0x0a000005) {
        return 0x0a000004;
    } else if (nic_ip_addr == 0x0a000006) {
        return 0x0a000005;
    } else if (nic_ip_addr == 0x0a000007) {
        return 0x0a000006;
    } else if (nic_ip_addr == 0x0a000008) {
        return 0x0a000007;
    } else if (nic_ip_addr == 0x0a000009) {
        return 0x0a000008;
    } else if (nic_ip_addr == 0x0a00000a) {
        return 0x0a000009;
    } else if (nic_ip_addr == 0x0a00000b) {
        return 0x0a00000a;
    } else if (nic_ip_addr == 0x0a00000c) {
        return 0x0a00000b;
    } else if (nic_ip_addr == 0x0a00000d) {
        return 0x0a00000c;
    } else if (nic_ip_addr == 0x0a00000e) {
        return 0x0a00000d;
    } else if (nic_ip_addr == 0x0a00000f) {
        return 0x0a00000e;
    } else if (nic_ip_addr == 0x0a000010) {
        return 0x0a00000f;
    } else if (nic_ip_addr == 0x0a000011) {
        return 0x0a000010;
    } else {
        return 0;
    }
}

int main(int argc, char** argv)
{
    uint64_t app_hdr;
    uint64_t dst_ip;
    uint64_t dst_context;
    int num_words;
    int i; 

    printf("Attempting to use uart\n");
    printf("Uart tx enabled is %#lx at %#lx\n", reg_read32(UART_BASE_ADDR + UART_TX_CTRL), UART_BASE_ADDR + UART_TX_CTRL);
    reg_write32(UART_BASE_ADDR + UART_TX_CTRL, 0b10000000000000011);
    reg_write32(UART_BASE_ADDR + UART_RX_CTRL, UART_TX_EN);
    printf("Uart tx enabled is %#lx at %#lx\n", reg_read32(UART_BASE_ADDR + UART_TX_CTRL), UART_BASE_ADDR + UART_TX_CTRL);
    
    reg_write32(UART_BASE_ADDR + UART_DIV, 868);
    printf("Uart div is %#lx\n", reg_read32(UART_BASE_ADDR + UART_DIV));

for (int i = 0; i < 128; i++) {
    while ((int32_t)reg_read32(UART_BASE_ADDR + UART_TX_FIFO) < 0);
    reg_write8(UART_BASE_ADDR + UART_TX_FIFO, 'a');
    while ((int32_t)reg_read32(UART_BASE_ADDR + UART_TX_FIFO) < 0);
    reg_write8(UART_BASE_ADDR + UART_TX_FIFO, 'b');
    while ((int32_t)reg_read32(UART_BASE_ADDR + UART_TX_FIFO) < 0);
    reg_write8(UART_BASE_ADDR + UART_TX_FIFO, 'c');
    while ((int32_t)reg_read32(UART_BASE_ADDR + UART_TX_FIFO) < 0);
    reg_write8(UART_BASE_ADDR + UART_TX_FIFO, '\n');
    while ((int32_t)reg_read32(UART_BASE_ADDR + UART_TX_FIFO) < 0);
    reg_write8(UART_BASE_ADDR + UART_TX_FIFO, '\0');
}

    //volatile uint64_t uart_base = UART_BASE_ADDR;
    //volatile uint64_t uart_tx_ctrl = UART_TX_CTRL;
    //*(uint32_t*)(uart_base + uart_tx_ctrl) = UART_TX_EN;
    //*(uint32_t*)(uart_base + UART_RX_CTRL) = UART_TX_EN;
    //printf("Uart enabled\n");
    //volatile uint64_t uart_tx = uart_base + UART_TX_FIFO;

    //for (int i = 0; i < 100; i++) {
        //printf("Writing uart\n");
    //    while (*(int32_t*)uart_tx < 0);
    //    *(uint32_t*)uart_tx = 0xDEADBEEF;
    //}
    // while (*(int32_t*)uart_tx < 0);
    // *(uint32_t*)uart_tx = '\n';
    // while (*(int32_t*)uart_tx < 0);
    // *(uint32_t*)uart_tx = '\0';


    printf("Uart written\n");


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

    // Send the msg
    dst_context = 0;
    app_hdr = (dst_ip << 32) | (dst_context << 16) | (NUM_MSG_WORDS*8);
    printf("Sending message\n");
    lnic_write_r(app_hdr);
    for (i = 0; i < NUM_MSG_WORDS; i++) {
        lnic_write_r(i);
    }
    printf("Receiving message\n");
    // Receive the msg
    lnic_wait();
    app_hdr = lnic_read();
    printf("Past wait\n");
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
            return -1;
        }
    }

    while (1); 

    return 0;
}

