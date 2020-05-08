#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "lnic.h"

#define MSG_LEN 80

int main(void)
{
    uint64_t app_hdr;
    uint16_t msg_len;
    int num_words;
    int i; 

    // register context ID with L-NIC
    lnic_add_context(0, 0);

    // Send a small msg
    uint64_t dst_ip = 0x0a000001;
    uint64_t dst_context = 0;
    app_hdr = (dst_ip << 32) | (dst_context << 16) | MSG_LEN;
    lnic_write_r(app_hdr);
    num_words = MSG_LEN/LNIC_WORD_SIZE;
    if (MSG_LEN % LNIC_WORD_SIZE != 0) { num_words++; }
    for (i = 0; i < num_words; i++) {
        lnic_write_i(0);
    }

    // Receive the msg
    lnic_wait();
    app_hdr = lnic_read();
    msg_len = app_hdr & LEN_MASK;
    if (msg_len != MSG_LEN) {
        printf("Expected: msg_len = %d, Received: msg_len = %d", MSG_LEN, msg_len);
        return -1;
    }
    for (i = 0; i < num_words; i++) {
        lnic_read();
    }   

    return 0;
}

