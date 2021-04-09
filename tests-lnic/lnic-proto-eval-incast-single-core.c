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
#define REPEAT_256(X) REPEAT_128(X) REPEAT_128(X) // 2 pkt
#define REPEAT_512(X) REPEAT_256(X) REPEAT_256(X) // 4 pkt
#define REPEAT_768(X) REPEAT_512(X) REPEAT_256(X) // 6 pkt
#define REPEAT_1024(X) REPEAT_512(X) REPEAT_512(X) // 8 pkt
#define REPEAT_1280(X) REPEAT_1024(X) REPEAT_256(X) // 10 pkt
#define REPEAT_1536(X) REPEAT_1024(X) REPEAT_512(X) // 12 pkt
#define REPEAT_1792(X) REPEAT_1024(X) REPEAT_768(X) // 14 pkt
#define REPEAT_2048(X) REPEAT_1024(X) REPEAT_1024(X) // 16 pkt
#define REPEAT_2304(X) REPEAT_1280(X) REPEAT_1024(X) // 18 pkt
#define REPEAT_2560(X) REPEAT_1280(X) REPEAT_1280(X) // 20 pkt
#define REPEAT_2816(X) REPEAT_2560(X) REPEAT_256(X) // 22 pkt
#define REPEAT_3072(X) REPEAT_2560(X) REPEAT_512(X) // 24 pkt
#define REPEAT_3328(X) REPEAT_2560(X) REPEAT_768(X) // 26 pkt
#define REPEAT_3584(X) REPEAT_2560(X) REPEAT_1024(X) // 28 pkt
#define REPEAT_3840(X) REPEAT_2560(X) REPEAT_1280(X) // 30 pkt
#define REPEAT_4096(X) REPEAT_2560(X) REPEAT_1536(X) // 32 pkt
#define REPEAT_4352(X) REPEAT_2560(X) REPEAT_1792(X) // 34 pkt
#define REPEAT_4608(X) REPEAT_2560(X) REPEAT_2048(X) // 36 pkt
#define REPEAT_4864(X) REPEAT_2560(X) REPEAT_2304(X) // 38 pkt

#define PKT_LEN_WORDS 128

#define NUM_CLIENTS 10

// IP addr's are assigned by firesim starting at 10.0.0.2. Server will be the first one.
uint64_t server_ip = 0x0a000002;

// Server expects messages of distinct sizes from each client
uint16_t expected_msg_len_pkts[] = {14, 16, 18, 20, 22, 24, 26, 28, 30, 32};

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
  uint16_t msg_len_words;
  uint16_t msg_len_bytes;
  uint64_t now;
  int i;

  dst_ip = server_ip;
  dst_context = 0;

  // wait for a bit to make sure the server is ready
  for (i = 0; i < 100; i++) {
    asm volatile("nop");
  }

  msg_len_words = expected_msg_len_pkts[client_ip_to_id(client_ip)] * PKT_LEN_WORDS;
  msg_len_bytes = msg_len_words * LNIC_WORD_SIZE;

  // Send msg to server
  printf("%ld: Client sending to server!\n", rdcycle());
  app_hdr = (dst_ip << 32) | (dst_context << 16) | (msg_len_bytes);
  lnic_write_r(app_hdr);
  // now = rdcycle();
  // lnic_write_r(now);
  // Optimize the msg sending logic for high throughput
  if (msg_len_words == 14 * PKT_LEN_WORDS) {
    goto write_14;
  } else if (msg_len_words == 16 * PKT_LEN_WORDS) {
    goto write_16;
  } else if (msg_len_words == 18 * PKT_LEN_WORDS) {
    goto write_18;
  } else if (msg_len_words == 20 * PKT_LEN_WORDS) {
    goto write_20;
  } else if (msg_len_words == 22 * PKT_LEN_WORDS) {
    goto write_22;
  } else if (msg_len_words == 24 * PKT_LEN_WORDS) {
    goto write_24;
  } else if (msg_len_words == 26 * PKT_LEN_WORDS) {
    goto write_26;
  } else if (msg_len_words == 28 * PKT_LEN_WORDS) {
    goto write_28;
  } else if (msg_len_words == 30 * PKT_LEN_WORDS) {
    goto write_30;
  } else if (msg_len_words == 32 * PKT_LEN_WORDS) {
    goto write_32;
  } else if (msg_len_words == 34 * PKT_LEN_WORDS) {
    goto write_34;
  } else if (msg_len_words == 36 * PKT_LEN_WORDS) {
    goto write_36;
  } else if (msg_len_words == 38 * PKT_LEN_WORDS) {
    goto write_38;
  } else {
    printf("Application is not throughput optimized for this message size: %d Bytes:\n", msg_len_words * LNIC_WORD_SIZE);
    for (i = 0; i < msg_len_words; i++) {
      lnic_write_r(i);
    }
  }

write_14: {
  REPEAT_1792(lnic_write_r(1);)
  goto lnic_finish;
}
write_16: {
  REPEAT_2048(lnic_write_r(1);)
  goto lnic_finish;
}
write_18: {
  REPEAT_2304(lnic_write_r(1);)
  goto lnic_finish;
}
write_20: {
  REPEAT_2560(lnic_write_r(1);)
  goto lnic_finish;
}
write_22: {
  REPEAT_2816(lnic_write_r(1);)
  goto lnic_finish;
}
write_24: {
  REPEAT_3072(lnic_write_r(1);)
  goto lnic_finish;
}
write_26: {
  REPEAT_3328(lnic_write_r(1);)
  goto lnic_finish;
}
write_28: {
  REPEAT_3584(lnic_write_r(1);)
  goto lnic_finish;
}
write_30: {
  REPEAT_3840(lnic_write_r(1);)
  goto lnic_finish;
}
write_32: {
  REPEAT_4096(lnic_write_r(1);)
  goto lnic_finish;
}
write_34: {
  REPEAT_4352(lnic_write_r(1);)
  goto lnic_finish;
}
write_36: {
  REPEAT_4608(lnic_write_r(1);)
  goto lnic_finish;
}
write_38: {
  REPEAT_4864(lnic_write_r(1);)
  goto lnic_finish;
}

lnic_finish:
  printf("&&CSV&&MsgSent,%ld,%d,%d\n", rdcycle(), client_ip_to_id(client_ip), msg_len_bytes);

  printf("Client %x complete!\n", client_ip);
  // Spin until the simulation is complete
  while(1);
  return 0; // will never get here
}
 
int run_server() {
  uint64_t app_hdr;
  uint16_t msg_len_bytes;
  int num_words;
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

  // receive msg from each client
  for (n = 0; n < NUM_CLIENTS; n++) {
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
    msg_arr_times[client_id] = rdcycle();
    msg_len[client_id] = msg_len_bytes;
    msg_count[client_id] += 1;

    // read all words of the msg
    num_words = msg_len_bytes/LNIC_WORD_SIZE;
    if (msg_len_bytes % LNIC_WORD_SIZE != 0) { num_words++; }
    if (num_words == 14 * PKT_LEN_WORDS) {
      goto read_14;
    } else if (num_words == 16 * PKT_LEN_WORDS) {
      goto read_16;
    } else if (num_words == 18 * PKT_LEN_WORDS) {
      goto read_18;
    } else if (num_words == 20 * PKT_LEN_WORDS) {
      goto read_20;
    } else if (num_words == 22 * PKT_LEN_WORDS) {
      goto read_22;
    } else if (num_words == 24 * PKT_LEN_WORDS) {
      goto read_24;
    } else if (num_words == 26 * PKT_LEN_WORDS) {
      goto read_26;
    } else if (num_words == 28 * PKT_LEN_WORDS) {
      goto read_28;
    } else if (num_words == 30 * PKT_LEN_WORDS) {
      goto read_30;
    } else if (num_words == 32 * PKT_LEN_WORDS) {
      goto read_32;
    } else if (num_words == 34 * PKT_LEN_WORDS) {
      goto read_34;
    } else if (num_words == 36 * PKT_LEN_WORDS) {
      goto read_36;
    } else if (num_words == 38 * PKT_LEN_WORDS) {
      goto read_38;
    } else {
      // printf("Application is not throughput optimized for this message size: %d Bytes:\n", msg_len_bytes);
      for (i = 0; i < num_words; i++) {
        lnic_read();
      }
    }

read_14: {
  REPEAT_1792(lnic_read();)
  goto lnic_done;
}
read_16: {
  REPEAT_2048(lnic_read();)
  goto lnic_done;
}
read_18: {
  REPEAT_2304(lnic_read();)
  goto lnic_done;
}
read_20: {
  REPEAT_2560(lnic_read();)
  goto lnic_done;
}
read_22: {
  REPEAT_2816(lnic_read();)
  goto lnic_done;
}
read_24: {
  REPEAT_3072(lnic_read();)
  goto lnic_done;
}
read_26: {
  REPEAT_3328(lnic_read();)
  goto lnic_done;
}
read_28: {
  REPEAT_3584(lnic_read();)
  goto lnic_done;
}
read_30: {
  REPEAT_3840(lnic_read();)
  goto lnic_done;
}
read_32: {
  REPEAT_4096(lnic_read();)
  goto lnic_done;
}
read_34: {
  REPEAT_4352(lnic_read();)
  goto lnic_done;
}
read_36: {
  REPEAT_4608(lnic_read();)
  goto lnic_done;
}
read_38: {
  REPEAT_4864(lnic_read();)
  goto lnic_done;
}

lnic_done:
  lnic_msg_done();

  }

  // make sure all msgs have been received
  for (n = 0; n < NUM_CLIENTS; n++) {
    if (msg_count[n] != 1) {
      printf("SERVER ERROR: incorrect msg_count for client %d, msg_count = %d\n", n, msg_count[n]);
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

