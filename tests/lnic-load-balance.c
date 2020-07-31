#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "lnic.h"
#include "lnic-scheduler.h"

// ------------------------------------------------------------
// Global information and definitions
// ------------------------------------------------------------

// Indicate to the nanokernel that this is a multicore program
bool is_single_core() { return false; }

// IP addr's are assigned by firesim starting at 10.0.0.2. Root will be the first one.
uint64_t root_addr = 0x0a000002;

// Expected address of the load generator
uint64_t load_gen_ip = 0x0a000001;

// Whether context 1 should sometimes stall a message
bool c1_stall = false;

// If context 1 stalls, it will stall for STALL_FACTOR times the message-specified stall duration
#define STALL_FACTOR 100

// If context 1 stalls, it will do so every STALL_FREQ messages
#define STALL_FREQ 10

// Types of tests to run
enum test_type_t {
  ONE_CONTEXT_FOUR_CORES, FOUR_CONTEXTS_FOUR_CORES, TWO_CONTEXTS_FOUR_SHARED_CORES,
  DIF_PRIORITY_LNIC_DRIVEN, DIF_PRIORITY_TIMER_DRIVEN, HIGH_PRIORITY_C1_STALL,
  LOW_PRIORITY_C1_STALL
};

char* name_for_test(enum test_type_t test_type) {
  if (test_type == ONE_CONTEXT_FOUR_CORES) {
    return "ONE_CONTEXT_FOUR_CORES";
  } else if (test_type == FOUR_CONTEXTS_FOUR_CORES) {
    return "FOUR_CONTEXTS_FOUR_CORES";
  } else if (test_type == TWO_CONTEXTS_FOUR_SHARED_CORES) {
    return "TWO_CONTEXTS_FOUR_SHARED_CORES";
  } else if (test_type == DIF_PRIORITY_LNIC_DRIVEN) {
    return "DIF_PRIORITY_LNIC_DRIVEN";
  } else if (test_type == DIF_PRIORITY_TIMER_DRIVEN) {
    return "DIF_PRIORITY_TIMER_DRIVEN";
  } else if (test_type == HIGH_PRIORITY_C1_STALL) {
    return "HIGH_PRIORITY_C1_STALL";
  } else if (test_type == LOW_PRIORITY_C1_STALL) {
    return "LOW_PRIORITY_C1_STALL";
  } else {
    return "UNKNOWN";
  }
}

// The actual test to run
enum test_type_t test_type = ONE_CONTEXT_FOUR_CORES;

int root_node(uint64_t argc, char** argv, int cid, int nc, uint64_t context_id, uint64_t priority);

// --------------------------------------------------------------------------
// Application entry point. Run setup and then become a leaf or a root node.
// --------------------------------------------------------------------------

int core_main(int argc, char** argv, int cid, int nc) {
    // Initialize variables and parse arguments
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

    // Start the test
    if (nic_ip_addr == root_addr) {
        // Only use the root node
        if (cid == 0) {
          printf("Root node running test type %s\n", name_for_test(test_type));
        }

        // Load balancing evals
        if (test_type == ONE_CONTEXT_FOUR_CORES) {
          // All cores have context ID 0, priority 0
          lnic_add_context(0, 0);
          return root_node(argc, argv, cid, nc, 0, 0);
        } else if (test_type == FOUR_CONTEXTS_FOUR_CORES) {
          // Context ID = Core ID, all priority 0
          lnic_add_context(cid, 0);
          return root_node(argc, argv, cid, nc, cid, 0);
        } else if (test_type == TWO_CONTEXTS_FOUR_SHARED_CORES) {
            // All cores run two threads: Context 0 priority 0, and context 1 priority 1
            scheduler_init();
            start_thread(root_node, 0, 0);
            start_thread(root_node, 1, 1);
            scheduler_run();
        }

        // Thread scheduling evals. These use only core 0.
        else if (test_type == DIF_PRIORITY_LNIC_DRIVEN) {
          // Core 0 runs two threads: Context 0 priority 0, and context 1 priority 1
          if (cid != 0) {
            return 0;
          }
          scheduler_init();
          start_thread(root_node, 0, 0);
          start_thread(root_node, 1, 1);
          scheduler_run();
        } else if (test_type == DIF_PRIORITY_TIMER_DRIVEN) {
          // Core 0 runs two threads: Context 0 priority 0, and context 1 priority 1
          // We use RTC timer interrupts instead of lnic interrupts though
          if (cid != 0) {
            return 0;
          }
          scheduler_init();
          start_thread(root_node, 0, 0);
          start_thread(root_node, 1, 1);
          scheduler_run_timer();
        } else if (test_type == HIGH_PRIORITY_C1_STALL) {
          // Core 0 runs two threads, context 0 and context 1, both with priority 0
          // Context 1 will stall STALL_FACTOR times longer than it's supposed to, once every STALL_FREQ messages
          if (cid != 0) {
            return 0;
          }
          c1_stall = true; // TODO: This should really be a preprocessor macro
          scheduler_init();
          start_thread(root_node, 0, 0);
          start_thread(root_node, 1, 0);
          scheduler_run();
        } else if (test_type == LOW_PRIORITY_C1_STALL) {
          // Core 0 runs two threads, context 0 and context 1, both with priority 1
          // Context 1 will stall STALL_FACTOR times longer than it's supposed to, once every STALL_FREQ messages
          if (cid != 0) {
            return 0;
          }
          c1_stall = true; // TODO: This should really be a preprocessor macro
          scheduler_init();
          start_thread(root_node, 0, 1);
          start_thread(root_node, 1, 1);
          scheduler_run();
        } else {
          printf("Unknown test type %d.\n", test_type);
          return -1;
        }
    } else {
        printf("Non-root node, not used in this test.\n");
        while (1);
    }
    printf("Load balance program should never reach here.\n");
    return -1;
}

void send_startup_msg(int cid) {
    uint64_t app_hdr = (load_gen_ip << 32) | (0 << 16) | (2*8);
    uint64_t empty_data = 0;
    uint64_t cid_to_send = cid;
    lnic_write_r(app_hdr);
    lnic_write_r(cid_to_send);
    lnic_write_r(empty_data);
}

// ----------------------------------------------
// Node-specific root function.
// ----------------------------------------------

int root_node(uint64_t argc, char** argv, int cid, int nc, uint64_t context_id, uint64_t priority) {
    // Set up service-local and core- and service-local data
    uint64_t app_hdr, rx_src_ip, rx_src_context, rx_msg_len, service_time, 
        sent_time, msgs_since_last_stall, start_stall_time;
    msgs_since_last_stall = 0;

    send_startup_msg(cid);

    while (1) {
      // Wait for a message to process
      lnic_wait();
      app_hdr = lnic_read();

      // Process the header
      // Check src IP
      rx_src_ip = (app_hdr & IP_MASK) >> 32;
      if (rx_src_ip != load_gen_ip) {
          printf("Root node received address not from load generator: %lx\n", rx_src_ip);
          return -1;
      }
      // Check src context
      rx_src_context = (app_hdr & CONTEXT_MASK) >> 16;
      if (rx_src_context != 0) {
          printf("Expected: src_context = %ld, Received: rx_src_context = %ld\n", 0, rx_src_context);
          return -1;
      }
      // Check msg length
      rx_msg_len = app_hdr & LEN_MASK;
      if (rx_msg_len != 2*8) {
          printf("Expected: msg_len = %d, Received: msg_len = %d\n", 2*8, rx_msg_len);
          return -1;
      }

      // Process the message data
      service_time = lnic_read();
      sent_time = lnic_read();

      // Exit if specified
      if (service_time == -1) {
        break;
      }

      // Stall if enabled
      if (c1_stall && context_id == 1) {
        if (msgs_since_last_stall >= STALL_FREQ) {
          msgs_since_last_stall = 0;
          service_time *= STALL_FACTOR;
        } else {
          msgs_since_last_stall++;
        }
      }

      // Stall for the message-specified cycles
      start_stall_time = read_csr(mcycle);
      while (read_csr(mcycle) < start_stall_time + service_time);

      // Write the message back out
      lnic_write_r(app_hdr);
      lnic_write_r(service_time);
      lnic_write_r(sent_time);
      lnic_msg_done();
    }
    return 0;
}
