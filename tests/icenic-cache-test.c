#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "mmio.h"
#include "icenic.h"
#include "encoding.h"

// For icenic, the whole message will first be brought in through DMA
// After that, to have the smallest possible memory footprint and minimize branches, a single temporary
// will be used to slowly swap each of the words that needs to move to the other end of the buffer before continuing
// We might need some receive header adjustments to get everything here to work.

#define IPV4_MIN_HEADER_SIZE 20

#define MSG_SIZE_WORDS 9 // We need one extra here since the timestamp word has to fit in the buffer

#define BUF_SIZE (ETH_HEADER_SIZE + IPV4_MIN_HEADER_SIZE + LNIC_HEADER_SIZE + MSG_SIZE_WORDS*sizeof(uint64_t))
uint8_t buffer[BUF_SIZE];

int main() {
    struct eth_header *eth;
    struct ipv4_header *ipv4;
    struct lnic_header *lnic;
    uint32_t tmp_ip_addr;
    uint16_t tmp_lnic_addr;
    uint64_t tmp_word;
    uint64_t proc_time_start, proc_time_end;
    eth = buffer;
    ipv4 = buffer + ETH_HEADER_SIZE;
    lnic = buffer + ETH_HEADER_SIZE + IPV4_MIN_HEADER_SIZE;
    uint64_t* buf_words = (uint64_t*)(buffer + ETH_HEADER_SIZE + IPV4_MIN_HEADER_SIZE + LNIC_HEADER_SIZE);
    uint64_t macaddr_long = nic_macaddr();
    uint8_t *mac = (uint8_t *) &macaddr_long;

    // Direct loop
    nic_recv(buffer);
    proc_time_start = rdcycle();
    memcpy(eth->dst_mac, eth->src_mac, MAC_ADDR_SIZE);
    memcpy(eth->src_mac, mac, MAC_ADDR_SIZE);
    tmp_ip_addr = ipv4->dst_addr;
    ipv4->dst_addr = ipv4->src_addr;
    ipv4->src_addr = tmp_ip_addr;
    tmp_lnic_addr = lnic->dst;
    lnic->dst = lnic->src;
    lnic->src = tmp_lnic_addr;
    proc_time_end = rdcycle();
    nic_send(buffer, BUF_SIZE);
    uint64_t proc_time_diff = proc_time_end - proc_time_start;
    return 0;
    //printf("Header processing time is %d\n", proc_time_diff);

    // Hold first word
    nic_recv(buffer);
    memcpy(eth->dst_mac, eth->src_mac, MAC_ADDR_SIZE);
    memcpy(eth->src_mac, mac, MAC_ADDR_SIZE);
    tmp_ip_addr = ipv4->dst_addr;
    ipv4->dst_addr = ipv4->src_addr;
    ipv4->src_addr = tmp_ip_addr;
    tmp_lnic_addr = lnic->dst;
    lnic->dst = lnic->src;
    lnic->src = tmp_lnic_addr;
    tmp_word = buf_words[MSG_SIZE_WORDS - 2];
    buf_words[MSG_SIZE_WORDS - 2] = buf_words[0];
    buf_words[0] = tmp_word;
    nic_send(buffer, BUF_SIZE);

    // Hold first two words
    nic_recv(buffer);
    memcpy(eth->dst_mac, eth->src_mac, MAC_ADDR_SIZE);
    memcpy(eth->src_mac, mac, MAC_ADDR_SIZE);
    tmp_ip_addr = ipv4->dst_addr;
    ipv4->dst_addr = ipv4->src_addr;
    ipv4->src_addr = tmp_ip_addr;
    tmp_lnic_addr = lnic->dst;
    lnic->dst = lnic->src;
    lnic->src = tmp_lnic_addr;
    tmp_word = buf_words[MSG_SIZE_WORDS - 3];
    buf_words[MSG_SIZE_WORDS - 3] = buf_words[0];
    buf_words[0] = tmp_word;
    tmp_word = buf_words[MSG_SIZE_WORDS - 2];
    buf_words[MSG_SIZE_WORDS - 2] = buf_words[1];
    buf_words[1] = tmp_word;
    nic_send(buffer, BUF_SIZE);

    // Hold first four words
    nic_recv(buffer);
    memcpy(eth->dst_mac, eth->src_mac, MAC_ADDR_SIZE);
    memcpy(eth->src_mac, mac, MAC_ADDR_SIZE);
    tmp_ip_addr = ipv4->dst_addr;
    ipv4->dst_addr = ipv4->src_addr;
    ipv4->src_addr = tmp_ip_addr;
    tmp_lnic_addr = lnic->dst;
    lnic->dst = lnic->src;
    lnic->src = tmp_lnic_addr;
    tmp_word = buf_words[MSG_SIZE_WORDS - 5];
    buf_words[MSG_SIZE_WORDS - 5] = buf_words[0];
    buf_words[0] = tmp_word;
    tmp_word = buf_words[MSG_SIZE_WORDS - 4];
    buf_words[MSG_SIZE_WORDS - 4] = buf_words[1];
    buf_words[1] = tmp_word;
    tmp_word = buf_words[MSG_SIZE_WORDS - 3];
    buf_words[MSG_SIZE_WORDS - 3] = buf_words[2];
    buf_words[2] = tmp_word;
    tmp_word = buf_words[MSG_SIZE_WORDS - 2];
    buf_words[MSG_SIZE_WORDS - 2] = buf_words[3];
    buf_words[3] = tmp_word;
    nic_send(buffer, BUF_SIZE);

    // Hold first 8 words
    nic_recv(buffer);
    memcpy(eth->dst_mac, eth->src_mac, MAC_ADDR_SIZE);
    memcpy(eth->src_mac, mac, MAC_ADDR_SIZE);
    tmp_ip_addr = ipv4->dst_addr;
    ipv4->dst_addr = ipv4->src_addr;
    ipv4->src_addr = tmp_ip_addr;
    tmp_lnic_addr = lnic->dst;
    lnic->dst = lnic->src;
    lnic->src = tmp_lnic_addr;
    tmp_word = buf_words[MSG_SIZE_WORDS - 9];
    buf_words[MSG_SIZE_WORDS - 9] = buf_words[0];
    buf_words[0] = tmp_word;
    tmp_word = buf_words[MSG_SIZE_WORDS - 8];
    buf_words[MSG_SIZE_WORDS - 8] = buf_words[1];
    buf_words[1] = tmp_word;
    tmp_word = buf_words[MSG_SIZE_WORDS - 7];
    buf_words[MSG_SIZE_WORDS - 7] = buf_words[2];
    buf_words[2] = tmp_word;
    tmp_word = buf_words[MSG_SIZE_WORDS - 6];
    buf_words[MSG_SIZE_WORDS - 6] = buf_words[3];
    buf_words[3] = tmp_word;
    tmp_word = buf_words[MSG_SIZE_WORDS - 5];
    buf_words[MSG_SIZE_WORDS - 5] = buf_words[4];
    buf_words[4] = tmp_word;
    tmp_word = buf_words[MSG_SIZE_WORDS - 4];
    buf_words[MSG_SIZE_WORDS - 4] = buf_words[5];
    buf_words[5] = tmp_word;
    tmp_word = buf_words[MSG_SIZE_WORDS - 3];
    buf_words[MSG_SIZE_WORDS - 3] = buf_words[6];
    buf_words[6] = tmp_word;
    tmp_word = buf_words[MSG_SIZE_WORDS - 2];
    buf_words[MSG_SIZE_WORDS - 2] = buf_words[7];
    buf_words[7] = tmp_word;
    nic_send(buffer, BUF_SIZE);

    // Hold first 16 words
    nic_recv(buffer);
    memcpy(eth->dst_mac, eth->src_mac, MAC_ADDR_SIZE);
    memcpy(eth->src_mac, mac, MAC_ADDR_SIZE);
    tmp_ip_addr = ipv4->dst_addr;
    ipv4->dst_addr = ipv4->src_addr;
    ipv4->src_addr = tmp_ip_addr;
    tmp_lnic_addr = lnic->dst;
    lnic->dst = lnic->src;
    lnic->src = tmp_lnic_addr;
    tmp_word = buf_words[MSG_SIZE_WORDS - 17];
    buf_words[MSG_SIZE_WORDS - 17] = buf_words[0];
    buf_words[0] = tmp_word;
    tmp_word = buf_words[MSG_SIZE_WORDS - 16];
    buf_words[MSG_SIZE_WORDS - 16] = buf_words[1];
    buf_words[1] = tmp_word;
    tmp_word = buf_words[MSG_SIZE_WORDS - 15];
    buf_words[MSG_SIZE_WORDS - 15] = buf_words[2];
    buf_words[2] = tmp_word;
    tmp_word = buf_words[MSG_SIZE_WORDS - 14];
    buf_words[MSG_SIZE_WORDS - 14] = buf_words[3];
    buf_words[3] = tmp_word;
    tmp_word = buf_words[MSG_SIZE_WORDS - 13];
    buf_words[MSG_SIZE_WORDS - 13] = buf_words[4];
    buf_words[4] = tmp_word;
    tmp_word = buf_words[MSG_SIZE_WORDS - 12];
    buf_words[MSG_SIZE_WORDS - 12] = buf_words[5];
    buf_words[5] = tmp_word;
    tmp_word = buf_words[MSG_SIZE_WORDS - 11];
    buf_words[MSG_SIZE_WORDS - 11] = buf_words[6];
    buf_words[6] = tmp_word;
    tmp_word = buf_words[MSG_SIZE_WORDS - 10];
    buf_words[MSG_SIZE_WORDS - 10] = buf_words[7];
    buf_words[7] = tmp_word;

    tmp_word = buf_words[MSG_SIZE_WORDS - 9];
    buf_words[MSG_SIZE_WORDS - 9] = buf_words[8];
    buf_words[8] = tmp_word;
    tmp_word = buf_words[MSG_SIZE_WORDS - 8];
    buf_words[MSG_SIZE_WORDS - 8] = buf_words[9];
    buf_words[9] = tmp_word;
    tmp_word = buf_words[MSG_SIZE_WORDS - 7];
    buf_words[MSG_SIZE_WORDS - 7] = buf_words[10];
    buf_words[10] = tmp_word;
    tmp_word = buf_words[MSG_SIZE_WORDS - 6];
    buf_words[MSG_SIZE_WORDS - 6] = buf_words[11];
    buf_words[11] = tmp_word;
    tmp_word = buf_words[MSG_SIZE_WORDS - 5];
    buf_words[MSG_SIZE_WORDS - 5] = buf_words[12];
    buf_words[12] = tmp_word;
    tmp_word = buf_words[MSG_SIZE_WORDS - 4];
    buf_words[MSG_SIZE_WORDS - 4] = buf_words[13];
    buf_words[13] = tmp_word;
    tmp_word = buf_words[MSG_SIZE_WORDS - 3];
    buf_words[MSG_SIZE_WORDS - 3] = buf_words[14];
    buf_words[14] = tmp_word;
    tmp_word = buf_words[MSG_SIZE_WORDS - 2];
    buf_words[MSG_SIZE_WORDS - 2] = buf_words[15];
    buf_words[15] = tmp_word;
    nic_send(buffer, BUF_SIZE);

    return 0;
}

