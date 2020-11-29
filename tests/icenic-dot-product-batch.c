#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "mmio.h"
#include "icenic.h"
#include "dot-product.h"

#define NUM_BUFS 30

/**
 * Compute dot product of msg data with in-memory data
 */

struct dot_product_header {
  uint64_t type;
};

struct config_header {
  uint64_t num_msgs;
  uint64_t timestamp;
};

struct data_header {
  uint64_t num_words;
};

struct resp_header {
  uint64_t result;
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
  struct dot_product_header *dot_prod;
  struct config_header *config;
  struct data_header *data;
  struct resp_header *resp;

  printf("Initializing...\n");
  // Initialize the working set
  uint64_t weights[NUM_WEIGHTS];
  for (i = 0; i < NUM_WEIGHTS; i++) {
    weights[i] = i;
  }

  printf("Ready!\n");
  while(1) {
    msg_cnt = 0;
    configured = 0;
    // wait for a Config msg
    while(!configured) {
      nic_recv_lnic(buffers[0], &lnic);
      dot_prod = (void *)lnic + LNIC_HEADER_SIZE;
      if (ntohl(dot_prod->type) != CONFIG_TYPE) {
        printf("Expected Config msg.\n");
        return -1;
      }
      config = (void *)dot_prod + sizeof(struct dot_product_header);
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
        dot_prod = (void *)lnic + LNIC_HEADER_SIZE;

        if (ntohl(dot_prod->type) != DATA_TYPE) {
          printf("Expected Data msg.\n");
          return -1;
        }
        data = (void *)dot_prod + sizeof(struct dot_product_header);

        // Compute dot product of msg with in-memory weights
        uint64_t num_words = ntohl(data->num_words);
        uint64_t result = 0;
        uint64_t *words = (void *)data + sizeof(struct data_header);
        int j;
        for (j = 0; j < num_words; j++) {
          uint64_t w = ntohl(words[j]);
          result += w * weights[w];
        }

        // send out response
        swap_eth(buffers[i]);
        dot_prod->type = htonl(RESP_TYPE);
        resp = (void *)data;
        resp->result = htonl(result);
        resp->timestamp = start_time;

        int resp_len = 34 + LNIC_HEADER_SIZE + sizeof(struct dot_product_header) + sizeof(struct resp_header);
        nic_post_send(buffers[i], resp_len);
        msg_cnt++;
      }
      // wait for all send operations to complete
      nic_wait_send_batch(NUM_BUFS);
    }
  }
  return 0;
}

