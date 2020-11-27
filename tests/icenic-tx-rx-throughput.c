#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "mmio.h"
#include "icenic.h"

/**
 * RX / TX Throughput tests for IceNIC
 */

#define START_RX_TYPE 0
#define START_TX_TYPE 1
#define DATA_TYPE 2
#define DONE_TYPE 3

#define TP_HEADER_SIZE 8
struct tp_header {
  uint64_t type;
};

#define START_RX_HEADER_SIZE 16
struct start_rx_header {
  uint64_t num_msgs;
  uint64_t timestamp;
};

#define START_TX_HEADER_SIZE 24
struct start_tx_header {
  uint64_t num_msgs;
  uint64_t msg_size;
  uint64_t timestamp;
};

#define DONE_HEADER_SIZE 8
struct done_header {
  uint64_t timestamp;
};

#define DONE_PKT_LEN (ETH_HEADER_SIZE + 20 + LNIC_HEADER_SIZE + TP_HEADER_SIZE + DONE_HEADER_SIZE)

uint64_t buffer[ETH_MAX_WORDS];

int main(void)
{
  // local variables
  int i;
  uint64_t start_time;
  uint64_t num_msgs;
  uint64_t msg_size;
  ssize_t size;

  // headers
  struct lnic_header *lnic;
  struct tp_header *tp;
  struct start_rx_header *start_rx;
  struct start_tx_header *start_tx;
  struct done_header *done;
  uint64_t *data_timestamp_ptr;

  while(1) {
    // wait for a START RX or START TX msg to arrive
    nic_recv_lnic(buffer, &lnic);
    tp = (void *)lnic + LNIC_HEADER_SIZE;
    if (ntohl(tp->type) == START_RX_TYPE) {
      // perform RX throughput test
      start_rx = (void *)tp + TP_HEADER_SIZE;
      num_msgs = ntohl(start_rx->num_msgs);
      start_time = ntohl(start_rx->timestamp);
      // read all msgs as fast as possible
      for (i = 0; i < num_msgs; i++) {
        nic_recv_lnic(buffer, &lnic);
      }
      // send DONE msg
      swap_addresses(buffer);
      tp->type = htonl(DONE_TYPE);
      done = (void *)tp + TP_HEADER_SIZE;
      done->timestamp = htonl(start_time);
      size = ceil_div(DONE_PKT_LEN, 8) * 8;
      nic_send(buffer, size);
    } else {
      // perform TX throughput test
      start_tx = (void *)tp + TP_HEADER_SIZE;
      num_msgs = ntohl(start_tx->num_msgs);
      msg_size = ntohl(start_tx->msg_size);
      start_time = ntohl(start_tx->timestamp);
      // generate all required msgs
      swap_addresses(buffer);
      tp->type = htonl(DATA_TYPE);
      data_timestamp_ptr = (void *)tp + msg_size - 8;
      (*data_timestamp_ptr) = htonl(start_time);
      size = ceil_div(ETH_HEADER_SIZE + 20 + LNIC_HEADER_SIZE + msg_size, 8) * 8;
      for (i = 0; i < num_msgs; i++) {
        nic_send(buffer, size);        
      }
    }
  }

  return 0;
}

