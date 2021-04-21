#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "lnic.h"

#define REPEAT_2(X) X X
#define REPEAT_4(X) REPEAT_2(X) REPEAT_2(X)
#define REPEAT_8(X) REPEAT_4(X) REPEAT_4(X)
#define REPEAT_16(X) REPEAT_8(X) REPEAT_8(X)
#define REPEAT_32(X) REPEAT_16(X) REPEAT_16(X)
#define REPEAT_64(X) REPEAT_32(X) REPEAT_32(X)
#define REPEAT_128(X) REPEAT_64(X) REPEAT_64(X) // 1 pkt

#define PKT_LEN_WORDS 128

#define NUM_CLIENTS 10

// IP addr's are assigned by firesim starting at 10.0.0.2. Server will be the first one.
uint64_t server_ip = 0x0a000002;

// Server expects messages of distinct sizes from each client
uint16_t expected_msg_len_pkts[] = {20, 22, 24, 26, 28, 30, 32, 34, 36, 38};

// use the last byte of the IP address to compute a unique ID for each client
uint8_t client_ip_to_id(uint32_t addr) {
  return ((uint8_t)addr) - 3;
}

bool is_client(uint32_t addr) {
  if ((addr > server_ip) && (addr <= (server_ip + NUM_CLIENTS))) {
    return true;
  }
  return false;
}

// Check if the provided address is assigned to an active node
// (i.e. either the server or a client)
bool is_active_ip(uint32_t addr) {
  if (addr == server_ip) {
    return true;
  }
  return is_client(addr);
}

int run_client(uint32_t client_ip) {
  uint64_t app_hdr;
  uint64_t dst_ip;
  uint64_t dst_context;
  uint16_t msg_len_pkts;
  uint16_t msg_len_words;
  uint16_t msg_len_bytes;
  uint8_t client_id;
  // uint64_t now;
  int i;

  client_id = client_ip_to_id(client_ip);
  dst_ip = server_ip;
  dst_context = 0;

  // wait for a bit to make sure the server is ready
  for (i = 0; i < 100 + 10 * client_id; i++) {
    asm volatile("nop");
  }

  msg_len_pkts = expected_msg_len_pkts[client_id];
  msg_len_words = msg_len_pkts * PKT_LEN_WORDS;
  msg_len_bytes = msg_len_words * LNIC_WORD_SIZE;

  // Send msg to server
  printf("%ld: Client sending to server!\n", rdcycle());
  app_hdr = (dst_ip << 32) | (dst_context << 16) | (msg_len_bytes);
  lnic_write_r(app_hdr);

  for (i = 0; i < msg_len_pkts; i++) {
    REPEAT_128(lnic_write_i(1);)
  }

  printf("&&CSV&&MsgSent,%ld,%d,%d\n", rdcycle(), client_id, msg_len_bytes);

  printf("Client %x complete!\n", client_ip);
  // Spin until the simulation is complete
  while(1);

  return 0; // will never get here
}
 
int run_server() {
  uint64_t app_hdr;
  uint16_t msg_len_bytes;
  // int num_words;
  int i;
  int n;
  // state to keep track of the number of msgs received from each client
  int msg_count[NUM_CLIENTS];
  uint64_t msg_arr_times[NUM_CLIENTS];
  uint16_t msg_len[NUM_CLIENTS];
  for (n = 0; n < NUM_CLIENTS; n++) {
    msg_count[n] = 0;
    msg_arr_times[n] = 0;
    msg_len[n] = 0;
  }

  bool all_received = false;

  // receive msg from each client
  // for (n = 0; n < NUM_CLIENTS; n++) {
  while (!all_received) {
    lnic_wait();
    app_hdr = lnic_read();

    // Check src IP
    uint64_t src_ip = (app_hdr & IP_MASK) >> 32;
    if (!is_client(src_ip)) {
        printf("SERVER ERROR: Received msg from non-client IP: src_ip = %lx\n", src_ip);
    }
    // Check src context
    uint64_t src_context = (app_hdr & CONTEXT_MASK) >> 16;
    if (src_context != 0) {
        printf("SERVER ERROR: Expected: correct_src_context = %d, Received: src_context = %ld\n", 0, src_context);
        // return -1;
    }
    // Check msg length
    msg_len_bytes = app_hdr & LEN_MASK;
    if (msg_len_bytes % (LNIC_WORD_SIZE * PKT_LEN_WORDS) != 0 ) {
        printf("SERVER ERROR: Received: msg_len_bytes = %d\n doesn't consist of full sized packets", msg_len_bytes);
        return -1;
    }
    uint16_t msg_len_pkts = msg_len_bytes / (LNIC_WORD_SIZE * PKT_LEN_WORDS);
    bool unexpected_msg = true;
    for (n = 0; n < NUM_CLIENTS; n++) {
        if (expected_msg_len_pkts[n] == msg_len_pkts) {
            unexpected_msg = false;
            break;
        }
    }
    if (unexpected_msg) {
        printf("SERVER ERROR: Received: msg_len_pkts = %d, Expected: either one of ", msg_len_pkts);
        for (n = 0; n < NUM_CLIENTS; n++) {
            printf(" %d", expected_msg_len_pkts[n]);
        }
        printf("\n");
        return -1;
    }

    uint8_t client_id = client_ip_to_id(src_ip);
    // mark msg as received
    if (msg_arr_times[client_id] == 0) {
      msg_arr_times[client_id] = rdcycle();
      msg_len[client_id] = msg_len_bytes;
    }
    msg_count[client_id] += 1;

    for (i = 0; i < msg_len_pkts; i++) {
      REPEAT_128(lnic_read();)
    }

    lnic_msg_done();

    all_received = true;
    for (n = 0; n < NUM_CLIENTS; n++) {
      if (msg_arr_times[n] == 0) {
          all_received = false;
          break;
      }
    }
  }

  // make sure all msgs have been received
  for (n = 0; n < NUM_CLIENTS; n++) {
    if (msg_count[n] != 1) {
      printf("SERVER ERROR: incorrect msg_count for client %d, msg_count = %d\n", n, msg_count[n]);
      printf("&&CSV&&MsgRcvdWithError,%ld,%d,%d\n", msg_arr_times[n], n, msg_len[n]);
    }
    else {
      printf("&&CSV&&MsgRcvd,%ld,%d,%d\n", msg_arr_times[n], n, msg_len[n]);
    }
  }

  // make sure RX queue is empty
  if (lnic_ready()) {
    printf("SERVER ERROR: RX Queue is not empty after processing all msgs!\n");
  }

  printf("Server complete!\n");
  while(1);
  return 0;
}

// Only use core 0, context 0
int main(int argc, char** argv) {
  uint64_t context_id = 0;
  uint64_t priority = 0;

  if (argc != 3) {
    printf("This program requires passing the L-NIC MAC address, followed by the L-NIC IP address.\n");
    return -1;
  }

  printf("___Starting Workload___\n");

  char* nic_ip_str = argv[2];
  uint32_t nic_ip_addr_lendian = 0;
  int retval = inet_pton4(nic_ip_str, nic_ip_str + strlen(nic_ip_str), &nic_ip_addr_lendian);

  // Risc-v is little-endian, but we store ip's as big-endian since the NIC works in big-endian
  uint32_t nic_ip_addr = swap32(nic_ip_addr_lendian);
  if (retval != 1 || nic_ip_addr == 0) {
      printf("Supplied NIC IP address is invalid.\n");
      return -1;
  }
  // Non-active nodes should just spin for the duration of the simulation
  if (!is_active_ip(nic_ip_addr)) {
    printf("This node is not active! Will spin indefinitely\n");
    while(1);
  }

  lnic_add_context(context_id, priority);

  int ret = 0;
  if (nic_ip_addr == server_ip) {
    printf("__Starting Server Node__\n");
    ret = run_server();
  } else if (is_client(nic_ip_addr)) {
    printf("__Starting Client Node__\n");
    ret = run_client(nic_ip_addr);
  }
 
  return ret;
}

