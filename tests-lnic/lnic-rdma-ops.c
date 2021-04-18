#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "lnic.h"

/* Implement a simple RDMA-like client and server.
 * The client sends messages that contain one-sided RDMA opcodes
 * (e.g., read, write, compare-and-swap, indirect-read).
 * The server processes each request and return a response.
 */

#define NUM_REQUESTS 100
#define MEM_SIZE 10
// one test for each RDMA operation type
#define NUM_TESTS 5

// SETUP msg:
// - msg_type
// - addr
#define SETUP_TYPE 0
#define SETUP_MSG_LEN 8*2

// READ msg:
// - msg_type
// - addr - address to read at server
#define READ_TYPE 1
#define READ_MSG_LEN 8*2

// WRITE msg:
// - msg_type
// - addr - address to write at server
// - new_val - new value to write at addr
#define WRITE_TYPE 2
#define WRITE_MSG_LEN 8*3

// CAS msg (compare-and-swap):
// - msg_type
// - addr - address to check / write at server
// - cmp_val - value to check against. If cmp_val == *addr, then write new_val to addr
// - new_val - new value to write if condition is met
#define CAS_TYPE 3
#define CAS_MSG_LEN 8*4

// FA msg (fetch-and-add):
// - msg_type
// - addr - address to increment
// - inc_val - amount to increment by
#define FA_TYPE 4
#define FA_MSG_LEN 8*3

// INDIRECT_READ msg:
// - msg_type
// - addr - address that contains pointer to value we want to read at the server
#define INDIRECT_READ_TYPE 5
#define INDIRECT_READ_MSG_LEN 8*2

// RESP msg:
// - msg_type
// - result - result of performing the requested operation
#define RESP_TYPE 6
#define RESP_MSG_LEN 8*2

// IP addr's are assigned by firesim starting at 10.0.0.2.
// Server will be the first one and client will be the second.
uint64_t server_ip = 0x0a000002;
uint64_t client_ip = 0x0a000003;

int get_test_name(int msg_type, char *test_name) {
  if (msg_type == READ_TYPE) {
    strcpy(test_name, "READ");
  } else if (msg_type == WRITE_TYPE) {
    strcpy(test_name, "WRITE");
  } else if (msg_type == CAS_TYPE) {
    strcpy(test_name, "CAS");
  } else if (msg_type == FA_TYPE) {
    strcpy(test_name, "FA");
  } else if (msg_type == INDIRECT_READ_TYPE) {
    strcpy(test_name, "INDIRECT_READ");
  }
  return 0;
}

// Check if the provided address is assigned to an active node
// (i.e. either the server or the client)
bool is_active_ip(uint32_t addr) {
  if (addr == server_ip || addr == client_ip) {
    return true;
  }
  return false;
}

int check_app_hdr(uint64_t app_hdr, uint64_t exp_src_ip,
    uint64_t exp_src_context, uint16_t exp_msg_len) {
  // Check src IP
  uint64_t src_ip = (app_hdr & IP_MASK) >> 32;
  if (src_ip != exp_src_ip) {
      printf("ERROR: unexpected src_ip: %lx\n", src_ip);
      return -1;
  }
  // Check src context
  uint64_t src_context = (app_hdr & CONTEXT_MASK) >> 16;
  if (src_context != exp_src_context) {
      printf("ERROR: unexpected src_context: %ld\n", src_context);
      return -1;
  }
  // Check msg length
  uint16_t msg_len = app_hdr & LEN_MASK;
  if (msg_len != exp_msg_len) {
      printf("ERROR: unexpected msg_len: %d\n", msg_len);
      return -1;
  }
  return 0; 
}

void send_request(uint64_t addr, uint64_t msg_type) {
  uint64_t app_hdr;
  uint64_t dst_ip = server_ip;

  if (msg_type == READ_TYPE) {
    app_hdr = (dst_ip << 32) | READ_MSG_LEN;
    lnic_write_r(app_hdr);
    lnic_write_r(msg_type);
    lnic_write_r(addr);
  } else if (msg_type == WRITE_TYPE) {
    app_hdr = (dst_ip << 32) | WRITE_MSG_LEN;
    lnic_write_r(app_hdr);
    lnic_write_r(msg_type);
    lnic_write_r(addr);
    lnic_write_i(0); // new_val
  } else if (msg_type == CAS_TYPE) {
    app_hdr = (dst_ip << 32) | CAS_MSG_LEN;
    lnic_write_r(app_hdr);
    lnic_write_r(msg_type);
    lnic_write_r(addr);
    lnic_write_i(0); // cmp_val
    lnic_write_i(0); // new_val
  } else if (msg_type == FA_TYPE) {
    app_hdr = (dst_ip << 32) | FA_MSG_LEN;
    lnic_write_r(app_hdr);
    lnic_write_r(msg_type);
    lnic_write_r(addr);
    lnic_write_i(1); // inc_val
  } else if (msg_type == INDIRECT_READ_TYPE) {
    app_hdr = (dst_ip << 32) | INDIRECT_READ_MSG_LEN;
    lnic_write_r(app_hdr);
    lnic_write_r(msg_type);
    lnic_write_r(addr + 64); // use addr[1] rather than addr[0]
  } else {
    printf("ERROR: invalid msg_type: %ld\n", msg_type);
  }
}

/* Client:
 * - First, wait for a message from the server which includes
 *   the address of an in-memory object that the client is allowed to
 *   operate on using RDMA ops.
 * - Send N requests to the server (one at a time) and record the
 *   end-to-end completion time.
 * - Print all completion times.
 */
int run_client() {
  uint64_t app_hdr;
  uint64_t start, end;
  int i, n;
  uint64_t msg_type;
  uint64_t addr; // addr of data on server to perform RDMA ops on
  uint64_t latencies[NUM_TESTS][NUM_REQUESTS];
  // collect all measurements are printf with this one buffer
  char output_buf[NUM_TESTS][(NUM_REQUESTS + 1)*10];

  // Wait to receive SETUP msg from server, which contains address
  // of in-memory object that can be operated upon.
  lnic_wait();
  app_hdr = lnic_read();
  check_app_hdr(app_hdr, server_ip, 0, SETUP_MSG_LEN);
  msg_type = lnic_read();
  if (msg_type != SETUP_TYPE) {
    printf("ERROR: received unexpected msg_type: %ld\n", msg_type);
    return -1;
  }
  addr = lnic_read();

  // Run all the tests
  for (n = 0; n < NUM_TESTS; n++) {
    // Send N requests to the server and record end-to-end response time
    for (i = 0; i < NUM_REQUESTS; i++) {
      start = rdcycle();
      send_request(addr, n+1);
      // wait for response
      lnic_wait();
      end = rdcycle();
      // process response
      app_hdr = lnic_read();
      check_app_hdr(app_hdr, server_ip, 0, RESP_MSG_LEN);
      lnic_read(); // msg_type
      lnic_read(); // result
      lnic_msg_done();
      // record latency
      latencies[n][i] = end - start;
    }
  }
  printf("Measurements complete!\n");

  // Print latency measurements
  for (n = 0; n < NUM_TESTS; n++) {
    uint32_t len_written;
    char test_name[32];
    get_test_name(n+1, test_name);
    len_written = sprintf(output_buf[n], "&&CSV&&,%s", test_name);
    for (i = 0; i < NUM_REQUESTS; i++) {
      len_written += sprintf(output_buf[n] + len_written, ",%ld", latencies[n][i]);
    }
    len_written += sprintf(output_buf[n] + len_written, "\n");
    printf("%s", output_buf[n]);
  }
  printf("Client complete!\n");
  return 0;
}

/* Server:
 * - First, initialize in-memory object and share ptr with client.
 * - Loop forever, serving RDMA requests.
 */ 
int run_server() {
  uint64_t app_hdr;
  uint64_t msg_type;
  uint64_t *addr;
  int i;

  // Initialize the data that we will let the client manipulate with RDMA ops.
  uint64_t data[MEM_SIZE];
  for (i = 0 ; i < MEM_SIZE; i++) {
    data[i] = (uint64_t)(&(data[i]));
  }

  // Send SETUP msg with a ptr to data
  printf("Server sending SETUP msg!\n");
  app_hdr = (client_ip << 32) | SETUP_MSG_LEN;
  lnic_write_r(app_hdr);
  lnic_write_i(SETUP_TYPE);
  lnic_write_r(data);

  // Loop forever processing client requests.
  while (1) {
    lnic_wait();
    app_hdr = lnic_read();
    msg_type = lnic_read();
    addr = (uint64_t *)lnic_read();
    // check that the provided address is valid
    if ( (addr < data) || (addr > &(data[MEM_SIZE-1])) ) {
      printf("ERROR: server received request with invalid addr: %ln\n", addr);
      return -1;
    }
    // write the start of the response msg
    lnic_write_r((app_hdr & (IP_MASK | CONTEXT_MASK)) | RESP_MSG_LEN);
    lnic_write_r(RESP_TYPE);
    // perform the requested operation and finish writing the response
    if (msg_type == READ_TYPE) {
      lnic_write_r(*addr);
    } else if (msg_type == WRITE_TYPE) {
      *addr = lnic_read();
      lnic_write_r(*addr);
    } else if (msg_type == CAS_TYPE) {
      // conditionally update addr
      if (*addr == lnic_read()) {
        *addr = lnic_read();
      } else {
         lnic_read();
      }
      lnic_write_r(*addr);
    } else if (msg_type == FA_TYPE) {
      *addr += lnic_read();
      lnic_write_r(*addr);
    } else if (msg_type == INDIRECT_READ_TYPE) {
      // addr is actually a pointer to the data that want to read
      uint64_t *ptr;
      ptr = (uint64_t *)(*addr); // cast the data at addr to a ptr
      lnic_write_r(*ptr);
    } else {
      printf("ERROR: server received invalid msg_type: %ld\n", msg_type);
    }
    lnic_msg_done();
  }

  return 0;
}

// Only use core 0, context 0
int main(uint64_t argc, char** argv) {
  uint64_t context_id = 0;
  uint64_t priority = 0;

  if (argc != 3) {
    printf("This program requires passing the L-NIC MAC address, followed by the L-NIC IP address.\n");
    return -1;
  }

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
    while(1);
  }

  lnic_add_context(context_id, priority);

  int ret = 0;
  if (nic_ip_addr == server_ip) {
    ret = run_server();
  } else if (nic_ip_addr == client_ip) {
    ret = run_client();
  }
 
  return ret;
}

