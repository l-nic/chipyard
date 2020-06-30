#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "lnic.h"

// TODO: This number is just copied from icenic. Figure out where it actually came from.
#define LNIC_BASE 0x10016000L
#define SIMPLENIC_SEND_REQ (SIMPLENIC_BASE + 0)
#define SIMPLENIC_RECV_REQ (SIMPLENIC_BASE + 8)
#define SIMPLENIC_SEND_COMP (SIMPLENIC_BASE + 16)
#define SIMPLENIC_RECV_COMP (SIMPLENIC_BASE + 18)
#define SIMPLENIC_COUNTS (SIMPLENIC_BASE + 20)
#define SIMPLENIC_MACADDR (SIMPLENIC_BASE + 24)

#define NUM_MSG_WORDS 10

int main(void)
{
    uint64_t app_hdr;
    uint64_t dst_ip;
    uint64_t dst_context;
    int num_words;
    int i; 

    printf("Starting program\n");
    // uint8_t recv_buf[100];
    // getstr(&recv_buf[0], 100);
    // printf("Received data\n");
    // for (int i = 0; i < 100; i++) {
    //     printf("Char %d: %c\n", i, recv_buf[i]);
    // }
    uint8_t mainvars[1000];
    getmainvars(&mainvars[0], 1000);
    for (int i = 0; i < 100; i++) {
        printf("Mainvars %d is %c\n", i, mainvars[i]);
    }

    // register context ID with L-NIC
    lnic_add_context(0, 0);

    // Send the msg
    dst_ip = 0x0a000002;
    dst_context = 0;
    app_hdr = (dst_ip << 32) | (dst_context << 16) | (NUM_MSG_WORDS*8);
    printf("Sending message\n");
    lnic_write_r(app_hdr);
    for (i = 0; i < NUM_MSG_WORDS; i++) {
        lnic_write_r(i);
    }
    printf("Finished sending message\n");
    printf("Receiving message\n");
    // Receive the msg
    lnic_wait();
    app_hdr = lnic_read();
    printf("Past wait\n");
    // Check dst IP
    uint64_t rx_dst_ip = (app_hdr & IP_MASK) >> 32;
    if (rx_dst_ip != dst_ip) {
        printf("Expected: dst_ip = %lx, Received: dst_ip = %lx\n", dst_ip, rx_dst_ip);
        return -1;
    }
    // Check dst context
    uint64_t rx_dst_context = (app_hdr & CONTEXT_MASK) >> 16;
    if (rx_dst_context != dst_context) {
        printf("Expected: dst_context = %ld, Received: dst_context = %ld\n", dst_context, rx_dst_context);
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

    return 0;
}

