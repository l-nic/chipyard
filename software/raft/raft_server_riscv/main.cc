#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <raft/raft.h>
#include <set>

using namespace std;

// Utility symbols linked into the binary
extern "C" {
	extern int inet_pton4 (const char *src, const char *end, unsigned char *dst);
	extern uint32_t swap32(uint32_t in);
    extern int _end;
    char* data_end = (char*)&_end + 16384*4;
    extern void sbrk_init(long int* init_addr);
    extern void __libc_init_array();
    extern void __libc_fini_array();
}

// Raft server constants
const uint32_t kBaseClusterIpAddr = 0xa000002;
const uint64_t kRaftElectionTimeoutMsec = 5;

// Global data
typedef struct {
    set<uint32_t> peer_ip_addrs;
    uint32_t own_ip_addr;
    raft_server_t *raft;
} server_t;

server_t server;
server_t *sv = &server;

int __raft_send_requestvote(raft_server_t* raft, void *user_data, raft_node_t *node, msg_requestvote_t* m) {

}

int __raft_send_appendentries(raft_server_t* raft, void *user_data, raft_node_t *node, msg_appendentries_t* m) {

}

int __raft_applylog(raft_server_t* raft, void *udata, raft_entry_t *ety) {

}

int __raft_persist_vote(raft_server_t *raft, void *udata, const int voted_for) {

}

int __raft_persist_term(raft_server_t* raft, void* udata, const int current_term) {

}

int __raft_logentry_offer(raft_server_t* raft, void *udata, raft_entry_t *ety, int ety_idx) {

}

int __raft_logentry_poll(raft_server_t* raft, void *udata, raft_entry_t *entry, int ety_idx) {

}

int __raft_logentry_pop(raft_server_t* raft, void *udata, raft_entry_t *entry, int ety_idx) {

}

void __raft_node_has_sufficient_logs(raft_server_t* raft, void *user_data, raft_node_t* node) {

}

void __raft_log(raft_server_t* raft, raft_node_t* node, void *udata, const char *buf) {

}

raft_cbs_t raft_funcs = {
    .send_requestvote            = __raft_send_requestvote,
    .send_appendentries          = __raft_send_appendentries,
    .applylog                    = __raft_applylog,
    .persist_vote                = __raft_persist_vote,
    .persist_term                = __raft_persist_term,
    .log_offer                   = __raft_logentry_offer,
    .log_poll                    = __raft_logentry_poll,
    .log_pop                     = __raft_logentry_pop,
    .node_has_sufficient_logs    = __raft_node_has_sufficient_logs,
    .log                         = __raft_log,
};


int client_main() {
    return 0;
}

int server_main() {
    printf("Starting raft server at ip %#lx\n", sv->own_ip_addr);
    sv->raft = raft_new();
    raft_set_election_timeout(sv->raft, kRaftElectionTimeoutMsec);
    raft_set_callbacks(sv->raft, &raft_funcs, (void*)sv);
    return 0;
}

int main(int argc, char** argv) {
    // Setup the C++ libraries
    sbrk_init((long int*)data_end);
    atexit(__libc_fini_array);
    __libc_init_array();

    // Initialize variables and parse arguments
    if (argc < 3) {
        printf("This program requires passing the L-NIC MAC address, followed by the L-NIC IP address.\n");
        return -1;
    }
    char* nic_ip_str = argv[2];
    uint32_t nic_ip_addr_lendian = 0;
    int retval = inet_pton4(nic_ip_str, nic_ip_str + strlen(nic_ip_str), (unsigned char*)&nic_ip_addr_lendian);

    // Risc-v is little-endian, but we store ip's as big-endian since the NIC works in big-endian
    uint32_t nic_ip_addr = swap32(nic_ip_addr_lendian);
    if (retval != 1 || nic_ip_addr == 0) {
        printf("Supplied NIC IP address is invalid.\n");
        return -1;
    }
    sv->own_ip_addr = nic_ip_addr;
    for (int i = 3; i < argc; i++) {
        char* ip_str = argv[i];
        uint32_t ip_lendian = 0;
        int retval = inet_pton4(ip_str, ip_str + strlen(ip_str), (unsigned char*)&ip_lendian);
        uint32_t peer_ip = swap32(ip_lendian);
        if (retval != 1 || peer_ip == 0) {
            printf("Peer IP address is invalid.\n");
            return -1;
        }
        sv->peer_ip_addrs.insert(peer_ip);
    }

    // Determine if client or server. Client will have an ip that is not a member of the peer ip set.
    if (sv->peer_ip_addrs.count(sv->own_ip_addr) == 0) {
        // This is a client
        return client_main();
    } else {
        // This is a server
        return server_main();
    }


    return 0;
}