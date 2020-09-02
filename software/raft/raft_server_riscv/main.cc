#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <cassert>
#include <raft/raft.h>
#include <vector>
#include <string>
#include <sstream>

#include "../../../tests/lnic.h"
#include "../../../tests/lnic-scheduler.h"

#include "mica/table/fixedtable.h"
#include "mica/util/hash.h"

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
const uint64_t kRaftElectionTimeoutMsec = 5000;
const uint64_t kCyclesPerMsec = 10000;
const uint64_t kAppKeySize = 16;
const uint64_t kAppValueSize = 64;
const uint64_t kAppNumKeys = 8*1024; // 8K keys

struct MyFixedTableConfig {
  static constexpr size_t kBucketCap = 7;

  // Support concurrent access. The actual concurrent access is enabled by
  // concurrent_read and concurrent_write in the configuration.
  static constexpr bool kConcurrent = false;

  // Be verbose.
  static constexpr bool kVerbose = false;

  // Collect fine-grained statistics accessible via print_stats() and
  // reset_stats().
  static constexpr bool kCollectStats = false;

  static constexpr size_t kKeySize = 8;

  //static std::string tableName = "test_table";
  static constexpr bool concurrentRead = false;
  static constexpr bool concurrentWrite = false;
  //static constexpr size_t itemCount = 640000;
  //static constexpr size_t itemCount = 16000;
  static constexpr size_t itemCount = 8*1024;
};

typedef mica::table::FixedTable<MyFixedTableConfig> FixedTable;
typedef mica::table::Result MicaResult;

template <typename T>
static uint64_t mica_hash(const T* key, size_t key_length) {
    return ::mica::util::hash(key, key_length);
}

// Global data

typedef struct {
    bool in_use = false;
    uint64_t header;
    uint64_t start_word;
    msg_entry_response_t msg_entry_response;
} leader_saveinfo_t;

typedef struct {
    vector<uint32_t> peer_ip_addrs;
    uint32_t own_ip_addr;
    uint32_t num_servers;
    raft_server_t *raft;
    uint64_t last_cycles;
    leader_saveinfo_t leader_saveinfo;
    uint32_t client_current_leader_index;
    vector<raft_entry_t*> log_record;
    FixedTable *table;
} server_t;

typedef enum ClientRespType {
    kSuccess, kFailRedirect, kFailTryAgain
};

typedef enum ReqType {
    kRequestVote, kAppendEntries, kClientReq, kRequestVoteResponse, kAppendEntriesResponse, kClientReqResponse
};

typedef struct __attribute__((packed)) {
    uint64_t key[kAppKeySize / sizeof(uint64_t)];
    uint64_t value[kAppValueSize / sizeof(uint64_t)];
    string to_string() const {
        ostringstream ret;
        ret << "[Key (";
        for (uint64_t k : key) ret << std::to_string(k) << ", ";
        ret << "), Value (";
        for (uint64_t v : value) ret << std::to_string(v) << ", ";
        ret << ")]";
        return ret.str();
    }
} client_req_t;

typedef struct __attribute__((packed)) {
    ClientRespType resp_type;
    uint32_t leader_ip;
} client_resp_t;

server_t server;
server_t *sv = &server;

uint32_t get_random();

void send_message(uint32_t dst_ip, uint64_t* buffer, uint32_t buf_words);

int __raft_send_requestvote(raft_server_t* raft, void *user_data, raft_node_t *node, msg_requestvote_t* m) {
    uint32_t dst_ip = raft_node_get_id(node);
    printf("Requesting vote from %#x\n", dst_ip); // TODO: Modify these structures to encode the application header data without needing the copies
    uint32_t buf_size = sizeof(msg_requestvote_t) + sizeof(uint64_t);
    if (buf_size % sizeof(uint64_t) != 0)
        buf_size += sizeof(uint64_t) - (buf_size % sizeof(uint64_t));
    char* buffer = malloc(buf_size);
    uint32_t msg_id = ReqType::kRequestVote;
    uint32_t src_ip = sv->own_ip_addr; // TODO: The NIC will eventually handle this for us
    memcpy(buffer, &msg_id, sizeof(uint32_t));
    memcpy(buffer + sizeof(uint32_t), &src_ip, sizeof(uint32_t));
    memcpy(buffer + sizeof(uint64_t), m, sizeof(msg_requestvote_t));
    send_message(dst_ip, (uint64_t*)buffer, buf_size);
    free(buffer);
    return 0;
}

int __raft_send_appendentries(raft_server_t* raft, void *user_data, raft_node_t *node, msg_appendentries_t* m) {
    //printf("Sending append entries\n");
    uint32_t buf_size = sizeof(msg_appendentries_t) + sizeof(uint64_t);
    for (int i = 0; i < m->n_entries; i++) {
        //printf("Entry size is %d\n", m->entries[i].data.len);
        buf_size += sizeof(msg_entry_t) + m->entries[i].data.len;
    }
    if (buf_size % sizeof(uint64_t) != 0)
        buf_size += sizeof(uint64_t) - (buf_size % sizeof(uint64_t));
    char* buffer = malloc(buf_size);
    char* buf_now = buffer;

    // Copy in extra header metadata
    uint32_t msg_id = ReqType::kAppendEntries;
    uint32_t src_ip = sv->own_ip_addr; // TODO: The NIC will eventually handle this for us
    memcpy(buf_now, &msg_id, sizeof(uint32_t));
    buf_now += sizeof(uint32_t);
    memcpy(buf_now, &src_ip, sizeof(uint32_t));
    buf_now += sizeof(uint32_t);

    // Copy in the appendentries structure, preserving the local entries pointer
    memcpy(buf_now, m, sizeof(msg_appendentries_t));
    msg_appendentries_t* buf_appendentries = (msg_appendentries_t*)buf_now;
    buf_appendentries->entries = nullptr;
    buf_now += sizeof(msg_appendentries_t);

    // Copy in the per-entry information
    for (int i = 0; i < m->n_entries; i++) {
        msg_entry_t* current_entry = &m->entries[i];
        memcpy(buf_now, current_entry, sizeof(msg_entry_t));
        msg_entry_t* buf_entry = (msg_entry_t*)buf_now;
        buf_entry->data.buf = nullptr;
        buf_now += sizeof(msg_entry_t);
        
        // Copy in the entry data
        // TODO: The entry data format should be more specifically defined
        memcpy(buf_now, m->entries[i].data.buf, m->entries[i].data.len);
        buf_now += m->entries[i].data.len;
    }

    uint32_t dst_ip = raft_node_get_id(node);
    send_message(dst_ip, (uint64_t*)buffer, buf_size);
    free(buffer);
    //printf("Sent appendentries\n");

    return 0;
}

int __raft_applylog(raft_server_t* raft, void *udata, raft_entry_t *ety) {
    assert(!raft_entry_is_cfg_change(ety));
    assert(ety->data.len == sizeof(client_req_t));
    client_req_t* client_req = (client_req_t*)ety->data.buf;
    assert(client_req->key[0] == client_req->value[0]); // This isn't a requirement. It's just how the test is currently set up.
    printf("Trying to apply log\n");
    uint64_t key_hash = mica_hash(&client_req->key[0], sizeof(uint64_t));
    FixedTable::ft_key_t ft_key;
    ft_key.qword[0] = client_req->key[0];
    MicaResult out_result = sv->table->set(key_hash, ft_key, (char*)(&client_req->value[0]));
    assert(out_result == MicaResult::kSuccess);
    return 0;
}

int __raft_persist_vote(raft_server_t *raft, void *udata, const int voted_for) {
    //printf("Trying to persist vote\n");
    return 0;
}

int __raft_persist_term(raft_server_t* raft, void* udata, const int current_term) {
    //printf("Trying to persist term\n"); // TODO: This should probably actually do something. 
    return 0;
}

int __raft_logentry_offer(raft_server_t* raft, void *udata, raft_entry_t *ety, int ety_idx) {
    assert(!raft_entry_is_cfg_change(ety));
    printf("Entry length is %d\n", ety->data.len);
    printf("Struct length is %d\n", sizeof(client_req_t));
    assert(ety->data.len == sizeof(client_req_t));

    // Not truly persistent, but at least allows us to track an easily accessible log record
    sv->log_record.push_back(ety);

    // TODO: erpc does some tricks here with persistent memory. Do we need to do that?

    printf("Offered entry\n");
    return 0;
}

int __raft_logentry_poll(raft_server_t* raft, void *udata, raft_entry_t *entry, int ety_idx) {
    printf("This application does not support log compaction.\n");
    assert(false);
    return -1;
}

int __raft_logentry_pop(raft_server_t* raft, void *udata, raft_entry_t *entry, int ety_idx) {
    free(entry->data.buf); // TODO: This will only work as long as the data is heap-allocated
    printf("Popped entry.\n");
    sv->log_record.pop_back();
    return 0;
}

int __raft_node_has_sufficient_logs(raft_server_t* raft, void *user_data, raft_node_t* node) {
    printf("Checking sufficient logs\n");
    return 0;
}

void __raft_log(raft_server_t* raft, raft_node_t* node, void *udata, const char *buf) {
    //printf("raft log: %s\n", buf);
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

// TODO: Make sure all message structs are packed

int client_send_request(client_req_t* client_req) {
    // Send the request to the current raft leader
    uint32_t dst_ip = sv->peer_ip_addrs[sv->client_current_leader_index];
    printf("Client sending request to %#x\n", dst_ip); // TODO: Modify these structures to encode the application header data without needing the copies
    uint32_t buf_size = sizeof(client_req_t) + sizeof(uint64_t);
    if (buf_size % sizeof(uint64_t) != 0)
        buf_size += sizeof(uint64_t) - (buf_size % sizeof(uint64_t));
    char* buffer = malloc(buf_size);
    printf("Buf size is %d\n", buf_size);
    uint32_t msg_id = ReqType::kClientReq;
    uint32_t src_ip = sv->own_ip_addr; // TODO: The NIC will eventually handle this for us
    memcpy(buffer, &msg_id, sizeof(uint32_t));
    memcpy(buffer + sizeof(uint32_t), &src_ip, sizeof(uint32_t));
    memcpy(buffer + sizeof(uint64_t), client_req, sizeof(client_req_t));
    send_message(dst_ip, (uint64_t*)buffer, buf_size);
    free(buffer);
    printf("Sent message\n");

    // Receive the response from the cluster.
    // TODO: A real version would really need a timeout.
    while (!lnic_ready());
    uint64_t header = lnic_read();
    uint64_t start_word = lnic_read();
    uint16_t* start_word_arr = (uint16_t*)&start_word;
    uint16_t msg_type = start_word_arr[0];
    assert(msg_type == ReqType::kClientReqResponse);

    uint64_t msg_len_words_remaining = ((header & LEN_MASK) / sizeof(uint64_t)) - 1;
    char* msg_buf = malloc(header & LEN_MASK);
    char* msg_current = msg_buf;
    for (int i = 0; i < msg_len_words_remaining; i++) {
        uint64_t data = lnic_read();
        memcpy(msg_current, &data, sizeof(uint64_t));
        msg_current += sizeof(uint64_t);
    }
    client_resp_t* client_response = (client_resp_t*)msg_buf;
    printf("Message response type is %d\n", client_response->resp_type);
    if (client_response->resp_type == ClientRespType::kSuccess) {
        printf("Request commited.\n");
        return 0;
    } else if (client_response->resp_type == ClientRespType::kFailRedirect) {
        printf("Cached leader is %d is not correct. Redirecting to leader %d\n", sv->peer_ip_addrs[sv->client_current_leader_index], client_response->leader_ip);
        for (int i = 0; i < sv->peer_ip_addrs.size(); i++) {
            if (sv->peer_ip_addrs[i] == client_response->leader_ip) {
                sv->client_current_leader_index = i;
                return -1;
            }
        }
        printf("New suggested leader not a known cluster ip.\n");
        assert(false);
    } else if (client_response->resp_type == ClientRespType::kFailTryAgain) {
        printf("Request failed to commit. Trying again\n");
        return -1;
    }
}

// TODO: We don't use this right now, but it could still be useful for dealing with timeouts. We might want to make it more random though.
void change_leader_to_any() {
    if (sv->client_current_leader_index == sv->num_servers - 1) {
        sv->client_current_leader_index = 0;
    } else {
        sv->client_current_leader_index++;
    }
}

int client_main() {
    sv->client_current_leader_index = 0;
    while (true) {
        // Create a client request
        client_req_t client_req;
        uint64_t rand_key = get_random() & (kAppNumKeys - 1);
        client_req.key[0] = rand_key;
        client_req.value[0] = rand_key;

        int send_retval = 0;
        do {
            send_retval = client_send_request(&client_req);
        } while (send_retval != 0);

        

        while (1);
    }
}

void raft_init() {
    printf("Starting raft server at ip %#lx\n", sv->own_ip_addr);
    sv->raft = raft_new();
    sv->table = new FixedTable(kAppValueSize, 0);
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
        //printf("elapsed %ld\n", msec_elapsed);
        raft_periodic(sv->raft, msec_elapsed);
    } else {
        raft_periodic(sv->raft, 0);
    }
}

void send_message(uint32_t dst_ip, uint64_t* buffer, uint32_t buf_words) {
    uint64_t header = 0;
    header |= (uint64_t)dst_ip << 32;
    header |= (uint16_t)buf_words;// * sizeof(uint64_t);
    //printf("Writing header %#lx\n", header);
    lnic_write_r(header);
    for (int i = 0; i < buf_words / sizeof(uint64_t); i++) {
        lnic_write_r(buffer[i]);
    }
}

void send_client_response(uint64_t header, uint64_t start_word, ClientRespType resp_type, uint32_t leader_ip=0) {
    client_resp_t client_response;
    client_response.resp_type = resp_type;
    client_response.leader_ip = leader_ip;
    uint32_t src_ip = (start_word & 0xffffffff00000000) >> 32;
    uint32_t buf_size = sizeof(client_resp_t) + sizeof(uint64_t);
    if (buf_size % sizeof(uint64_t) != 0)
        buf_size += sizeof(uint64_t) - (buf_size % sizeof(uint64_t));
    char* buffer = malloc(buf_size);
    uint32_t msg_id = ReqType::kClientReqResponse;
    memcpy(buffer, &msg_id, sizeof(uint32_t));
    memcpy(buffer + sizeof(uint32_t), &sv->own_ip_addr, sizeof(uint32_t));
    memcpy(buffer + sizeof(uint64_t), &client_response, sizeof(client_resp_t));
    send_message(src_ip, (uint64_t*)buffer, buf_size);
}

uint32_t get_random() {
    return rand(); // TODO: Figure out what instruction this actually turns into
}

void service_client_message(uint64_t header, uint64_t start_word) {
    printf("Received client request with header %#lx\n", header);
    raft_node_t* leader = raft_get_current_leader_node(sv->raft);
    if (leader == nullptr) {
        // Cluster doesn't have a leader, reply with error.
        printf("Cluster has no leader, replying with error\n");
        uint64_t msg_len_words_remaining = ((header & LEN_MASK) / sizeof(uint64_t)) - 1;
        for (int i = 0; i < msg_len_words_remaining; i++) {
            volatile uint64_t dump = lnic_read();
        }
        send_client_response(header, start_word, ClientRespType::kFailTryAgain);
        return;
    }

    uint32_t leader_ip = raft_node_get_id(leader);
    if (leader_ip != sv->own_ip_addr) {
        // This is not the cluster leader, reply with actual leader.
        printf("This is not the cluster leader, redirecting to %#x\n", leader_ip);
        uint64_t msg_len_words_remaining = ((header & LEN_MASK) / sizeof(uint64_t)) - 1;
        for (int i = 0; i < msg_len_words_remaining; i++) {
            volatile uint64_t dump = lnic_read();
        }
        send_client_response(header, start_word, ClientRespType::kFailRedirect, leader_ip);
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
        uint64_t data = lnic_read();
        memcpy(msg_current, &data, sizeof(uint64_t));
        msg_current += sizeof(uint64_t);
    }

    

    // Set up saveinfo so that we can reply to the client later.
    leader_saveinfo_t &leader_sav = sv->leader_saveinfo;
    assert(!leader_sav.in_use);
    leader_sav.in_use = true;
    leader_sav.header = header;
    leader_sav.start_word = start_word;

    // Set up the raft entry
    msg_entry_t ent;
    ent.type = RAFT_LOGTYPE_NORMAL;
    ent.id = get_random(); // TODO: Check this!
    ent.data.buf = msg_buf + sizeof(uint64_t);
    ent.data.len = (header & LEN_MASK) - sizeof(uint64_t);
    printf("Header length is %d and entry length is %d\n", (header & LEN_MASK), ent.data.len);

    // Send the entry into the raft library handlers
    printf("Adding raft log entry\n");
    int raft_retval = raft_recv_entry(sv->raft, &ent, &leader_sav.msg_entry_response);
    assert(raft_retval == 0);
}

void service_request_vote(uint64_t header, uint64_t start_word) {
    uint64_t msg_len_words_remaining = ((header & LEN_MASK) / sizeof(uint64_t)) - 1;
    char* msg_buf = malloc(header & LEN_MASK);
    char* msg_current = msg_buf;
    memcpy(msg_current, &start_word, sizeof(uint64_t));
    msg_current += sizeof(uint64_t);
    for (int i = 0; i < msg_len_words_remaining; i++) {
        uint64_t data = lnic_read();
        memcpy(msg_current, &data, sizeof(uint64_t));
        msg_current += sizeof(uint64_t);
    }

    uint32_t src_ip = (start_word & 0xffffffff00000000) >> 32;
    //printf("Source ip is %x, node is %#lx\n", src_ip, raft_get_node(sv->raft, src_ip));
    msg_requestvote_response_t msg_response_buf;
    int raft_retval = raft_recv_requestvote(sv->raft, raft_get_node(sv->raft, src_ip), (msg_requestvote_t*)(msg_buf + sizeof(uint64_t)), &msg_response_buf);
    assert(raft_retval == 0);
    //printf("Received requestvote\n");
    free(msg_buf);

    // Send the response to the vote request
    uint32_t buf_size = sizeof(msg_requestvote_response_t) + sizeof(uint64_t);
    if (buf_size % sizeof(uint64_t) != 0)
        buf_size += sizeof(uint64_t) - (buf_size % sizeof(uint64_t));
    char* buffer = malloc(buf_size);
    uint32_t msg_id = ReqType::kRequestVoteResponse;
    memcpy(buffer, &msg_id, sizeof(uint32_t));
    memcpy(buffer + sizeof(uint32_t), &sv->own_ip_addr, sizeof(uint32_t));
    memcpy(buffer + sizeof(uint64_t), &msg_response_buf, sizeof(msg_requestvote_response_t));
    send_message(src_ip, (uint64_t*)buffer, buf_size);
    free(buffer);
}

void service_request_vote_response(uint64_t header, uint64_t start_word) {
    uint64_t msg_len_words_remaining = ((header & LEN_MASK) / sizeof(uint64_t)) - 1;
    char* msg_buf = malloc(header & LEN_MASK);
    char* msg_current = msg_buf;
    memcpy(msg_current, &start_word, sizeof(uint64_t));
    msg_current += sizeof(uint64_t);
    for (int i = 0; i < msg_len_words_remaining; i++) {
        uint64_t data = lnic_read();
        memcpy(msg_current, &data, sizeof(uint64_t));
        msg_current += sizeof(uint64_t);
    }

    uint32_t src_ip = (start_word & 0xffffffff00000000) >> 32;
    int raft_retval = raft_recv_requestvote_response(sv->raft, raft_get_node(sv->raft, src_ip), (msg_requestvote_response_t*)(msg_buf + sizeof(uint64_t)));
    assert(raft_retval == 0);
    //printf("Received requestvote response\n");
    free(msg_buf);
}

void service_append_entries(uint64_t header, uint64_t start_word) {
    uint64_t msg_len_words_remaining = ((header & LEN_MASK) / sizeof(uint64_t)) - 1;
    char* msg_buf = malloc(header & LEN_MASK);
    char* msg_current = msg_buf;
    memcpy(msg_current, &start_word, sizeof(uint64_t));
    msg_current += sizeof(uint64_t);
    for (int i = 0; i < msg_len_words_remaining; i++) {
        uint64_t data = lnic_read();
        memcpy(msg_current, &data, sizeof(uint64_t));
        msg_current += sizeof(uint64_t);
    }

    uint32_t src_ip = (start_word & 0xffffffff00000000) >> 32;

    msg_appendentries_t* append_entries = (msg_appendentries_t*)(msg_buf + sizeof(uint64_t));
    if (append_entries->n_entries > 0) {
        char* per_entry_buf = msg_buf + sizeof(uint64_t) + sizeof(msg_appendentries_t);
        append_entries->entries = malloc(sizeof(msg_entry_t)*append_entries->n_entries); // TODO: Get rid of these extra malloc's
        for (int i = 0; i < append_entries->n_entries; i++) {
            msg_entry_t* current_entry = &append_entries->entries[i];
            memcpy(current_entry, per_entry_buf, sizeof(msg_entry_t));
            per_entry_buf += sizeof(msg_entry_t);
            current_entry->data.buf = malloc(current_entry->data.len);
            memcpy(current_entry->data.buf, per_entry_buf, current_entry->data.len);
            per_entry_buf += current_entry->data.len;
        }
    }

    if (append_entries->n_entries != 0) {
        printf("Received non-zero number of entries\n");
    }

    //printf("Source ip is %x, node is %#lx\n", src_ip, raft_get_node(sv->raft, src_ip));
    msg_appendentries_response_t msg_response_buf;
    int raft_retval = raft_recv_appendentries(sv->raft, raft_get_node(sv->raft, src_ip), append_entries, &msg_response_buf);
    assert(raft_retval == 0);
    //printf("Received appendentries\n");
    free(msg_buf);

    // Send the response to the appendentries request
    uint32_t buf_size = sizeof(msg_appendentries_response_t) + sizeof(uint64_t);
    if (buf_size % sizeof(uint64_t) != 0)
        buf_size += sizeof(uint64_t) - (buf_size % sizeof(uint64_t));
    char* buffer = malloc(buf_size);
    uint32_t msg_id = ReqType::kAppendEntriesResponse;
    memcpy(buffer, &msg_id, sizeof(uint32_t));
    memcpy(buffer + sizeof(uint32_t), &sv->own_ip_addr, sizeof(uint32_t));
    memcpy(buffer + sizeof(uint64_t), &msg_response_buf, sizeof(msg_appendentries_response_t));
    send_message(src_ip, (uint64_t*)buffer, buf_size);
    free(buffer);
}

void service_append_entries_response(uint64_t header, uint64_t start_word) {
    //printf("Receiving appendentries response\n");
    uint64_t msg_len_words_remaining = ((header & LEN_MASK) / sizeof(uint64_t)) - 1;
    char* msg_buf = malloc(header & LEN_MASK);
    char* msg_current = msg_buf;
    memcpy(msg_current, &start_word, sizeof(uint64_t));
    msg_current += sizeof(uint64_t);
    for (int i = 0; i < msg_len_words_remaining; i++) {
        uint64_t data = lnic_read();
        memcpy(msg_current, &data, sizeof(uint64_t));
        msg_current += sizeof(uint64_t);
    }

    uint32_t src_ip = (start_word & 0xffffffff00000000) >> 32;
    //printf("Entering raft call with message of size %d\n", header & LEN_MASK);
    int raft_retval = raft_recv_appendentries_response(sv->raft, raft_get_node(sv->raft, src_ip), (msg_appendentries_response_t*)(msg_buf + sizeof(uint64_t)));
    //printf("raft retval is %d\n", raft_retval);
    assert(raft_retval == 0 || raft_retval == RAFT_ERR_NOT_LEADER);
    //printf("Server received append entries but is not the leader\n");
    //printf("Received appendentries response\n");
    free(msg_buf);
}

void service_pending_messages() {
    //lnic_wait();
    //uint64_t header = lnic_read();
    if (!lnic_ready()) {
        return;
    }
    uint64_t header = lnic_read();
    uint64_t start_word = lnic_read();
    uint16_t* start_word_arr = (uint16_t*)&start_word;
    uint16_t msg_type = start_word_arr[0];
    // printf("header is %#lx, start word is %#lx\n", header, start_word);

    if (msg_type == ReqType::kClientReq) {
        service_client_message(header, start_word);
    } else if (msg_type == ReqType::kAppendEntries) {
        service_append_entries(header, start_word);
    } else if (msg_type == ReqType::kRequestVote) {
        service_request_vote(header, start_word);
    } else if (msg_type == ReqType::kRequestVoteResponse) {
        service_request_vote_response(header, start_word);
    } else if (msg_type == ReqType::kAppendEntriesResponse) {
        service_append_entries_response(header, start_word);
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

        // Reply to clients if any entries have committed
        leader_saveinfo_t &leader_sav = sv->leader_saveinfo;
        //printf("Log has %d entries\n", sv->log_record.size());
        if (!leader_sav.in_use) {
            continue;
        }
        printf("Leader has saved a response\n");
        int commit_status = raft_msg_entry_response_committed(sv->raft, &leader_sav.msg_entry_response);
        assert(commit_status == 0 || commit_status == 1);
        if (commit_status == 1) {
            // We've already committed the entry
            raft_apply_all(sv->raft);
            leader_sav.in_use = false;
            send_client_response(leader_sav.header, leader_sav.start_word, ClientRespType::kSuccess, 0);
        }

        // raft_node_t* leader_node = raft_get_current_leader_node(sv->raft);
        // if (leader_node == nullptr) {
        //     continue;
        // }
        // uint32_t leader_ip = raft_node_get_id(leader_node);
        //printf("Current leader ip is %#x\n", leader_ip);
    }

    return 0;
}

int main(int argc, char** argv) {
    // Setup the C++ libraries
    sbrk_init((long int*)data_end);
    atexit(__libc_fini_array);
    __libc_init_array();

    // Initialize variables and parse arguments
    printf("Started raft main\n");
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
    bool is_server = false;
    for (int i = 3; i < argc; i++) {
        char* ip_str = argv[i];
        uint32_t ip_lendian = 0;
        int retval = inet_pton4(ip_str, ip_str + strlen(ip_str), (unsigned char*)&ip_lendian);
        uint32_t peer_ip = swap32(ip_lendian);
        if (retval != 1 || peer_ip == 0) {
            printf("Peer IP address is invalid.\n");
            return -1;
        }
        sv->peer_ip_addrs.push_back(peer_ip);
        if (peer_ip == sv->own_ip_addr) {
            is_server = true;
        }
    }
    sv->num_servers = sv->peer_ip_addrs.size();

    // Determine if client or server. Client will have an ip that is not a member of the peer ip set.
    if (!is_server) {
        // This is a client
        return client_main();
    } else {
        // This is a server
        return server_main();
    }


    return 0;
}