#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <cassert>
#include <raft/raft.h>
#include <set>

#include "../../../tests/lnic.h"
#include "../../../tests/lnic-scheduler.h"

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
const uint64_t kCyclesPerMsec = 1000;

// Global data

typedef struct {
    bool in_use = false;
    uint64_t header;
    msg_entry_response_t msg_entry_response;
} leader_saveinfo_t;

typedef struct {
    set<uint32_t> peer_ip_addrs;
    uint32_t own_ip_addr;
    uint32_t num_servers;
    raft_server_t *raft;
    uint64_t last_cycles;
    leader_saveinfo_t leader_saveinfo;
} server_t;

typedef enum ClientRespType {
    kSuccess, kFailRedirect, kFailTryAgain
};

typedef enum ReqType {
    kRequestVote, kAppendEntries, kClientReq
};

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

void raft_init() {
    printf("Starting raft server at ip %#lx\n", sv->own_ip_addr);
    sv->raft = raft_new();
    raft_set_election_timeout(sv->raft, kRaftElectionTimeoutMsec);
    raft_set_callbacks(sv->raft, &raft_funcs, (void*)sv);
    for (const auto& node_ip : sv->peer_ip_addrs) {
        if (node_ip == sv->own_ip_addr) {
            raft_add_node(sv->raft, nullptr, node_ip, 1);
        } else {
            raft_add_node(sv->raft, nullptr, node_ip, 0);
        }
    }
}

void periodic_raft_wrapper() {
    uint64_t cycles_now = csr_read(mcycle);
    uint64_t cycles_elapsed = cycles_now - sv->last_cycles;
    uint64_t msec_elapsed = cycles_elapsed / kCyclesPerMsec;
    if (msec_elapsed > 0) {
        sv->last_cycles = cycles_now;
        raft_periodic(sv->raft, msec_elapsed);
    } else {
        raft_periodic(sv->raft, 0);
    }
}

void send_client_response(uint64_t header, ClientRespType resp_type, uint32_t leader_ip=0) {

}

uint32_t get_random() {
    return 0; // TODO: Fix
}

void service_client_message(uint64_t header, uint64_t start_word) {
    printf("Received client request\n");
    raft_node_t* leader = raft_get_current_leader_node(sv->raft);
    if (leader == nullptr) {
        // Cluster doesn't have a leader, reply with error.
        printf("Cluster has no leader, replying with error\n");
        send_client_response(header, ClientRespType::kFailTryAgain);
        return;
    }

    uint32_t leader_ip = raft_node_get_id(leader);
    if (leader_ip != sv->own_ip_addr) {
        // This is not the cluster leader, reply with actual leader.
        printf("This is not the cluster leader, redirecting to %#x\n", leader_ip);
        send_client_response(header, ClientRespType::kFailRedirect, leader_ip);
        return;
    }

    // This is actually the leader

    // Read in the rest of the message. TODO: This should eventually be brought into the raft library or at least not done with malloc
    uint64_t msg_len_words_remaining = ((header & LEN_MASK) / sizeof(uint64_t)) - 1;
    char* msg_buf = malloc(header & LEN_MASK);
    char* msg_current = msg_buf;
    memcpy(msg_current, &start_word, sizeof(uint64_t));
    msg_current += sizeof(uint64_t);
    for (int i = 0; i < msg_len_words_remaining; i++) {
        uint64_t data = csr_read(0x50);
        memcpy(msg_current, &data, sizeof(uint64_t));
        msg_current += sizeof(uint64_t);
    }

    

    // Set up saveinfo so that we can reply to the client later.
    leader_saveinfo_t &leader_sav = sv->leader_saveinfo;
    assert(!leader_sav.in_use);
    leader_sav.in_use = true;
    leader_sav.header = header;

    // Set up the raft entry
    msg_entry_t ent;
    ent.type = RAFT_LOGTYPE_NORMAL;
    ent.id = get_random(); // TODO: Check this!
    ent.data.buf = msg_buf;
    ent.data.len = header & LEN_MASK;

    // Send the entry into the raft library handlers
    printf("Adding raft log entry\n");
    int raft_retval = raft_recv_entry(sv->raft, &ent, &leader_sav.msg_entry_response);
    assert(raft_retval == 0);
}

void service_pending_messages() {
    //lnic_wait();
    //uint64_t header = lnic_read();
    //while (csr_read(0x52) == 0); // Wait for a message to arrive
    if (csr_read(0x52) == 0) {
        return;
    }
    uint64_t header = csr_read(0x50);
    uint64_t start_word = csr_read(0x50);
    uint16_t* start_word_arr = (uint16_t*)&start_word;
    uint16_t msg_type = start_word_arr[0];

    if (msg_type == ReqType::kClientReq) {
        service_client_message(header, start_word);
    } else if (msg_type == ReqType::kAppendEntries) {

    } else if (msg_type == ReqType::kRequestVote) {

    } else {
        printf("Received unknown message type %d\n", msg_type);
        exit(-1);
    }
}

int server_main() {
    raft_init();

    while (true) {
        periodic_raft_wrapper();
        service_pending_messages();
    }

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
    sv->num_servers = sv->peer_ip_addrs.size();

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