#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "encoding.h"
#include "mmio.h"
#include "icenic.h"

#define NUM_BUFS 30
#define TSTAMP_BYTES 8

#define CONFIG_TYPE 0
#define DATA_TYPE 1

/**
 * Increment each word of DATA requests by 1 and forward result.
 */

struct stream_header {
  uint64_t type;
};

struct config_header {
  uint64_t num_msgs;
  uint64_t timestamp;
};

uint64_t buffers[NUM_BUFS][ETH_MAX_WORDS];

int main(void)
{
  int msg_cnt;
  uint64_t num_msgs;
  uint64_t start_time;
  int configured;
  int len;
  int i;

  // headers
  struct lnic_header *lnic;
  struct stream_header *stream;
  struct config_header *config;

  printf("Ready!\n");
  nic_boot_pkt();
  while(1) {
    msg_cnt = 0;
    configured = 0;
    // wait for a Config msg
    while(!configured) {
      nic_recv_lnic(buffers[0], &lnic);
      stream = (void *)lnic + LNIC_HEADER_SIZE;
      if (ntohl(stream->type) != CONFIG_TYPE) {
        printf("Expected Config msg.\n");
        return -1;
      }
      config = (void *)stream + sizeof(struct stream_header);
      num_msgs = ntohl(config->num_msgs);
      start_time = config->timestamp; // do not swap bytes
      configured = 1;
    }
    // process all messages
    while (msg_cnt < num_msgs) {
      // give the NIC all descriptors
      nic_post_recv_batch(buffers, NUM_BUFS);
      for (i = 0; i < NUM_BUFS; i++) {
        len = nic_wait_recv();

        lnic = (void *)buffers[i] + 34;
        stream = (void *)lnic + LNIC_HEADER_SIZE;

        if (ntohl(stream->type) != DATA_TYPE) {
          printf("Expected Data msg.\n");
          return -1;
        }

        // increment each word by one, except the first and the last (timestamp)
        uint64_t *words = (void *)stream + sizeof(struct stream_header);
        uint64_t num_words = ntohs(lnic->msg_len)/8;
        int j;
        for (j = 0; j < num_words-2; j++) {
          words[j] = htonl(ntohl(words[j]) + 1);
        }

        // send out response
        swap_eth(buffers[i]);
        // copy over start timestamp
        memcpy((void *)buffers[i] + len - TSTAMP_BYTES, &start_time, TSTAMP_BYTES);
        nic_post_send(buffers[i], len);
        msg_cnt++;
      }
      // wait for all send operations to complete
      nic_wait_send_batch(NUM_BUFS);
    }
  }
  return 0;
}

