#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "lnic.h"


#define NUM_MSG_WORDS 10
uint32_t own_id = 0;

uint32_t get_dst_ip() {
    if (own_id == 0) {
        return 0x0a000003;
    } else if (own_id == 1) {
        return 0x0a000002;
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

    if (argc != 2) {
        printf("This program requires one argument to specify its id\n");
        return -1;
    }
    char* own_id_str = argv[1];
    char* end;
    own_id = atol(own_id_str);
    dst_ip = get_dst_ip();
    if (dst_ip == 0) {
        printf("Could not find valid destination ip\n");
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
    if (rx_dst_ip != dst_ip) {
        printf("Expected: dst_ip = %lx, Received: dst_ip = %lx\n", dst_ip, rx_dst_ip);
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

    return 0;
}

