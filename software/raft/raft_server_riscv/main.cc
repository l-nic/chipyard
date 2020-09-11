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
#include <queue>
#include <map>
#include <bits/stdc++.h>

#include "../../../tests/lnic.h"
#include "../../../tests/lnic-scheduler.h"

#include "mica/table/fixedtable.h"
#include "mica/util/hash.h"

using namespace std;

uint64_t initial_msg_recv;

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

const uint32_t kStaticBufSize = 4;
msg_entry_t entry_static_buf[kStaticBufSize];

bool sent_startup_msg = false;

// Raft server constants
const uint32_t kBaseClusterIpAddr = 0xa000002;
const uint64_t kRaftElectionTimeoutMsec = 5;
const uint64_t kCyclesPerMsec = 3200000;
//const uint64_t kCyclesPerMsec = 320000;
const uint64_t kAppKeySize = 16; // 16B keys
const uint64_t kAppValueSize = 64; // 64B values
const uint64_t kAppNumKeys = 10000; // 10K keys
const uint32_t kStallLoopTurnsPerCount = 1000;
const uint32_t kMaxStaticPendingRequests = 10;

uint64_t load_gen_ip = 0x0a000001;

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
    uint64_t sent_time;
    msg_entry_response_t msg_entry_response;
    leader_saveinfo_t() {
        in_use = false;
    }
} leader_saveinfo_t;

typedef struct {
    vector<uint32_t> peer_ip_addrs;
    uint32_t own_ip_addr;
    uint32_t num_servers;
    raft_server_t *raft;
    uint64_t last_cycles;
    queue<leader_saveinfo_t> leader_saveinfo;
    uint32_t next_open_leader_sav_index;
    uint32_t max_pending_requests;
    uint32_t client_current_leader_index;
    vector<raft_entry_t*> log_record;
    FixedTable *table;
    uint32_t stall_count;
} server_t;

// Should have coverage for 1024 entries of 80 bytes each
const uint32_t kNumAllocSlots = 1024;

typedef struct {
    char* base_ptr;
    uint32_t current_index;
    bitset<kNumAllocSlots> used_slots;
} allocator_t;

allocator_t alloc_global;
allocator_t* alloc = &alloc_global;

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

client_req_t* get_slot() {
    assert(!alloc->used_slots.all());
    for (int i = alloc->current_index; i < kNumAllocSlots; i++) {
        if (alloc->used_slots[i]) {
            continue;
        }
        alloc->used_slots[i] = true;
        alloc->current_index = i + 1;
        if (alloc->current_index == kNumAllocSlots) {
            alloc->current_index = 0;
        }
        return (client_req_t*)(alloc->base_ptr + i*sizeof(client_req_t));
    }
    printf("Allocator should never reach here\n");
    assert(false);
    return 0;
}

void free_slot(client_req_t* addr) {
    uint32_t i = ((uint64_t)((char*)addr - alloc->base_ptr)) / sizeof(client_req_t);
    assert(alloc->used_slots[i]);
    alloc->used_slots[i] = false;
}

uint32_t get_random();

void send_message(uint32_t dst_ip, uint64_t* buffer, uint32_t buf_words);

int __raft_send_requestvote(raft_server_t* raft, void *user_data, raft_node_t *node, msg_requestvote_t* m) {
    uint32_t dst_ip = raft_node_get_id(node);
    //printf("Requesting vote from %#x\n", dst_ip); // TODO: Modify these structures to encode the application header data without needing the copies
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
    // TODO: Remove this temporary debug check
    //printf("Sending appendentries\n");
    // for (int i = 0; i < m->n_entries; i++) {
    //     printf("Checking assert\n");
    //     assert(m->entries[i].data.len == sizeof(client_req_t));
    // }

    // Prepare and send the header
    uint32_t dst_ip = raft_node_get_id(node);
    uint32_t buf_size = sizeof(msg_appendentries_t);
    buf_size += m->n_entries * (sizeof(msg_entry_t) + sizeof(client_req_t));
    uint64_t header = 0;
    header |= (uint64_t)dst_ip << 32;
    header |= (uint16_t)buf_size;
    lnic_write_r(header);

    // Prepare and send the main appendentries structure
    // This is a fixed size, so it's faster without a loop
    m->msg_id = ReqType::kAppendEntries;
    uint64_t* m_data = (uint64_t*)m;
    lnic_write_r(m_data[0]);
    lnic_write_r(m_data[1]);
    lnic_write_r(m_data[2]);
    lnic_write_r(m_data[3]);
    lnic_write_r(m_data[4]);
    lnic_write_r(m_data[5]);

    // Prepare and send the data for each entry
    for (int i = 0; i < m->n_entries; i++) {
        // Send the entry metadata
        msg_entry_t* current_entry = &m->entries[i]; 
        uint64_t* entry_data = (uint64_t*)current_entry;
        lnic_write_r(entry_data[0]);
        lnic_write_r(entry_data[1]);
        lnic_write_r(entry_data[2]);
        lnic_write_r(entry_data[3]);

        // Send the client request
        uint64_t* msg_data = (uint64_t*)current_entry->data.buf;
        lnic_write_r(msg_data[0]);
        lnic_write_r(msg_data[1]);
        lnic_write_r(msg_data[2]);
        lnic_write_r(msg_data[3]);
        lnic_write_r(msg_data[4]);
        lnic_write_r(msg_data[5]);
        lnic_write_r(msg_data[6]);
        lnic_write_r(msg_data[7]);
        lnic_write_r(msg_data[8]);
        lnic_write_r(msg_data[9]);
    }
    return 0;
}

int __raft_applylog(raft_server_t* raft, void *udata, raft_entry_t *ety) {
    uint64_t apply_start = csr_read(mcycle);
    assert(!raft_entry_is_cfg_change(ety));
    assert(ety->data.len == sizeof(client_req_t));
    client_req_t* client_req = (client_req_t*)ety->data.buf;
    assert(client_req->key[0] == client_req->value[0]); // This isn't a requirement. It's just how the test is currently set up.
    //printf("Trying to apply log\n");
    uint64_t key_hash = mica_hash(&client_req->key[0], sizeof(uint64_t));
    FixedTable::ft_key_t ft_key;
    ft_key.qword[0] = client_req->key[0];
    MicaResult out_result = sv->table->set(key_hash, ft_key, (char*)(&client_req->value[0]));
    assert(out_result == MicaResult::kSuccess);
    free_slot(client_req);
    uint64_t apply_end = csr_read(mcycle);
    //printf("apply elapsed %ld\n", apply_end - apply_start);
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
    //printf("Entry length is %d\n", ety->data.len);
    //printf("Struct length is %d\n", sizeof(client_req_t));
    assert(ety->data.len == sizeof(client_req_t));

    // Not truly persistent, but at least allows us to track an easily accessible log record
    //sv->log_record.push_back(ety);

    // TODO: erpc does some tricks here with persistent memory. Do we need to do that?

    //printf("Offered entry\n");
    return 0;
}

int __raft_logentry_poll(raft_server_t* raft, void *udata, raft_entry_t *entry, int ety_idx) {
    //printf("This application does not support log compaction.\n");
    assert(false);
    return -1;
}

int __raft_logentry_pop(raft_server_t* raft, void *udata, raft_entry_t *entry, int ety_idx) {
    free(entry->data.buf); // TODO: This will only work as long as the data is heap-allocated
    //printf("Popped entry.\n");
    //sv->log_record.pop_back();
    return 0;
}

int __raft_node_has_sufficient_logs(raft_server_t* raft, void *user_data, raft_node_t* node) {
    //printf("Checking sufficient logs\n");
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
    // Send the request to the current raft leader, and start tracking the time
    uint64_t dst_ip = sv->peer_ip_addrs[sv->client_current_leader_index];
    //printf("Client sending request to %#x\n", dst_ip); // TODO: Modify these structures to encode the application header data without needing the copies
    uint64_t start_time = csr_read(mcycle); // TODO: This isn't a great metric, but it's a start

    // Build and send the header and start word
    uint64_t header = 0;
    header |= (uint16_t)(sizeof(client_req_t) + sizeof(uint64_t));
    header |= (dst_ip << 32);
    lnic_write_r(header);
    uint64_t start_word = 0;
    start_word |= ReqType::kClientReq;
    lnic_write_r(start_word);

    // Send the actual request.
    uint64_t* req_data = (uint64_t*)client_req;
    lnic_write_r(req_data[0]);
    lnic_write_r(req_data[1]);
    lnic_write_r(req_data[2]);
    lnic_write_r(req_data[3]);
    lnic_write_r(req_data[4]);
    lnic_write_r(req_data[5]);
    lnic_write_r(req_data[6]);
    lnic_write_r(req_data[7]);
    lnic_write_r(req_data[8]);
    lnic_write_r(req_data[9]);

    // Receive the response from the cluster.
    // TODO: A real version would really need a timeout.
    lnic_wait();

    // Process the header and the start word
    header = lnic_read();
    start_word = lnic_read();
    uint16_t* start_word_arr = (uint16_t*)&start_word;
    uint16_t msg_type = start_word_arr[0];
    assert(msg_type == ReqType::kClientReqResponse);

    // Process the actual message response
    uint64_t msg_response_data = lnic_read();
    client_resp_t* client_response = (client_resp_t*)&msg_response_data;
    lnic_msg_done();

    // Log the elapsed time
    uint64_t finish_time = csr_read(mcycle);
    uint64_t elapsed_time = finish_time - start_time;
    printf("Message response type is %d, elapsed time is %ld cycles\n", client_response->resp_type, elapsed_time);

    // Take action depending on the response type
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

void send_startup_msg(int cid, uint64_t context_id) {
    uint64_t app_hdr = (load_gen_ip << 32) | (0 << 16) | (2*8);
    uint64_t cid_to_send = cid;
    lnic_write_r(app_hdr);
    lnic_write_r(cid_to_send);
    lnic_write_r(context_id);
}

int client_main() {
    //printf("Starting client main\n");

    // Client is now just a proxy client
    map<uint64_t, uint64_t> sent_map;
    send_startup_msg(0, 0);
    while (true) {
        lnic_wait();
        uint64_t header = lnic_read();
        uint32_t src_ip = (header >> 32) & 0xffffffff;
        if (src_ip == load_gen_ip) {
            // Read in message metadata from load generator
            uint64_t service_time = lnic_read();
            uint64_t sent_time = lnic_read();
            uint64_t start_word = lnic_read();
            sent_map[sent_time] = 0;

            // Verify the message has the correct formatting
            uint16_t msg_len = header & 0xffff;
            assert(msg_len == 3*sizeof(uint64_t) + sizeof(client_req_t));
            assert(src_ip == 0x0a000001);
            uint16_t* start_word_arr = (uint16_t*)&start_word;
            uint16_t msg_type = start_word_arr[0];
            assert(msg_type == ReqType::kClientReq);

            // Send the new header
            uint32_t* leader_ptr = (uint32_t*)&service_time;
            uint64_t leader_ip = leader_ptr[0];
            uint64_t new_header = 0;
            new_header |= sizeof(client_req_t) + 2*sizeof(uint64_t);
            new_header |= (leader_ip << 32);
            lnic_write_r(new_header);

            // Send the whole message
            uint64_t start_cycles = csr_read(mcycle);
            lnic_write_r(start_word);
            lnic_write_r(sent_time);
            lnic_copy();
            lnic_copy();
            lnic_copy();
            lnic_copy();
            lnic_copy();
            lnic_copy();
            lnic_copy();
            lnic_copy();
            lnic_copy();
            lnic_copy();
            lnic_msg_done();
            sent_map[sent_time] = start_cycles;
        } else {
            // Receive the response from the cluster.
            // TODO: A real version would really need a timeout.
            uint64_t end_cycles = csr_read(mcycle);
            uint16_t msg_len = header & 0xffff;
            assert(msg_len == sizeof(client_resp_t) + 2*sizeof(uint64_t));

            // Process the header and the start word
            uint64_t start_word = lnic_read();
            uint64_t sent_time = lnic_read();
            uint64_t start_cycles = sent_map.at(sent_time);
            uint16_t* start_word_arr = (uint16_t*)&start_word;
            uint16_t msg_type = start_word_arr[0];
            assert(msg_type == ReqType::kClientReqResponse);

            // Forward the new header and metadata
            uint64_t new_header = 0;
            new_header |= sizeof(client_resp_t) + 3*sizeof(uint64_t);
            new_header |= (load_gen_ip << 32);
            lnic_write_r(new_header);
            uint64_t delta_cycles = end_cycles - start_cycles;
            lnic_write_r(delta_cycles);
            lnic_write_r(sent_time);
            lnic_write_r(start_word);

            // Forward the rest of the response message
            lnic_copy();
            lnic_msg_done();
        }
    }


    // sv->client_current_leader_index = 0;
    // while (true) {
    //     // Create a client request
    //     client_req_t client_req;
    //     uint64_t rand_key = get_random() & (kAppNumKeys - 1);
    //     client_req.key[0] = rand_key;
    //     client_req.value[0] = rand_key;

    //     int send_retval = 0;
    //     do {
    //         send_retval = client_send_request(&client_req);
    //     } while (send_retval != 0);
    // }
}

void raft_init() {
    //printf("Starting raft server at ip %#lx\n", sv->own_ip_addr);
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

void __attribute__((noinline)) send_message(uint32_t dst_ip, uint64_t* buffer, uint32_t buf_words) {
    uint64_t header = 0;
    header |= (uint64_t)dst_ip << 32;
    header |= (uint16_t)buf_words;// * sizeof(uint64_t);
    //printf("Writing header %#lx\n", header);
    lnic_write_r(header);
    for (int i = 0; i < buf_words / sizeof(uint64_t); i++) {
        lnic_write_r(buffer[i]);
    }
}

void send_client_response(uint64_t header, uint64_t sent_time, ClientRespType resp_type, uint32_t leader_ip=0) {
    // Build and send the header and start word
    header &= 0xffffffffffff0000;
    header |= (uint16_t)(sizeof(client_resp_t) + 2*sizeof(uint64_t));
    lnic_write_r(header);
    uint64_t start_word = 0;
    start_word |= ReqType::kClientReqResponse;
    lnic_write_r(start_word);
    lnic_write_r(sent_time);

    // Build and send the client response
    client_resp_t client_response;
    client_response.resp_type = resp_type;
    client_response.leader_ip = leader_ip;
    uint64_t* client_response_data = (uint64_t*)&client_response;
    lnic_write_r(client_response_data[0]);
}

uint32_t get_random() {
    return rand(); // TODO: Figure out what instruction this actually turns into
}

void service_client_message(uint64_t header, uint64_t start_word) {
    //printf("Servicing a client message with header %#lx\n", header);
    raft_node_t* leader = raft_get_current_leader_node(sv->raft);
    uint64_t sent_time = lnic_read();
    if (leader == nullptr) {
        // Cluster doesn't have a leader, reply with error.
        // start_word is *not* part of the message structure when the client sends requests. This keeps us from having
        // to send extra data in every append entries request. Dump all 10 remaining message words.
        volatile uint64_t dump = lnic_read();
        dump = lnic_read();
        dump = lnic_read();
        dump = lnic_read();
        dump = lnic_read();
        dump = lnic_read();
        dump = lnic_read();
        dump = lnic_read();
        dump = lnic_read();
        dump = lnic_read();
        lnic_msg_done();
        send_client_response(header, sent_time, ClientRespType::kFailTryAgain);
        return;
    }

    uint32_t leader_ip = raft_node_get_id(leader);
    if (leader_ip != sv->own_ip_addr) {
        // This is not the cluster leader, reply with actual leader.
        volatile uint64_t dump = lnic_read();
        dump = lnic_read();
        dump = lnic_read();
        dump = lnic_read();
        dump = lnic_read();
        dump = lnic_read();
        dump = lnic_read();
        dump = lnic_read();
        dump = lnic_read();
        dump = lnic_read();
        lnic_msg_done();
        send_client_response(header, sent_time, ClientRespType::kFailRedirect, leader_ip);
        return;
    }

    // This is actually the leader

    // Read in the rest of the message. This unfortunately needs to use malloc so that it persists.
    client_req_t* req = get_slot();//malloc(sizeof(client_req_t));
    uint64_t* req_data = (uint64_t*)req;
    req_data[0] = lnic_read();
    req_data[1] = lnic_read();
    req_data[2] = lnic_read();
    req_data[3] = lnic_read();
    req_data[4] = lnic_read();
    req_data[5] = lnic_read();
    req_data[6] = lnic_read();
    req_data[7] = lnic_read();
    req_data[8] = lnic_read();
    req_data[9] = lnic_read();
    lnic_msg_done();

    // Select a saveinfo entry. Default to incrementing the entry counter, unless no more entries can be found.
    uint32_t index_to_use = sv->max_pending_requests;
    if (sv->next_open_leader_sav_index < sv->max_pending_requests) {
        index_to_use = sv->next_open_leader_sav_index;
        sv->next_open_leader_sav_index++;
    } else {
        for (int i = 0; i < sv->max_pending_requests; i++) {
            if (!sv->leader_saveinfo[i].in_use) {
                index_to_use = i;
                break;
            }
        }
    }

    if (index_to_use == sv->max_pending_requests) {
        sv->leader_saveinfo.push_back(leader_saveinfo_t());
        sv->max_pending_requests++;
        sv->next_open_leader_sav_index++;
    }
    assert(index_to_use < sv->max_pending_requests);

    // Set up saveinfo so that we can reply to the client later.
    leader_saveinfo_t &leader_sav = sv->leader_saveinfo[index_to_use];
    assert(!leader_sav.in_use);
    leader_sav.in_use = true;
    leader_sav.header = header;
    leader_sav.sent_time = sent_time;

    // Set up the raft entry
    msg_entry_t ent;
    ent.type = RAFT_LOGTYPE_NORMAL;
    ent.id = get_random(); // TODO: Check this!
    ent.data.buf = req;
    ent.data.len = sizeof(client_req_t);

    uint64_t entry_recv_start = csr_read(mcycle);

    // Send the entry into the raft library handlers
    int raft_retval = raft_recv_entry(sv->raft, &ent, &leader_sav.msg_entry_response);
    assert(raft_retval == 0);
    uint64_t entry_recv_end = csr_read(mcycle);
    //printf("entry recv cycled %ld\n", entry_recv_end - entry_recv_start);
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
    lnic_msg_done();

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
    lnic_msg_done();

    uint32_t src_ip = (start_word & 0xffffffff00000000) >> 32;
    int raft_retval = raft_recv_requestvote_response(sv->raft, raft_get_node(sv->raft, src_ip), (msg_requestvote_response_t*)(msg_buf + sizeof(uint64_t)));
    assert(raft_retval == 0);
    //printf("Received requestvote response\n");
    free(msg_buf);
}

void service_append_entries(uint64_t header, uint64_t start_word) {
    //printf("Received append entries from %x\n", (header & 0xffffffff00000000) >> 32);
    // Read in the appendentries structure
    msg_appendentries_t m;
    uint64_t* m_data = (uint64_t)&m;
    m_data[0] = start_word;
    m_data[1] = lnic_read();
    m_data[2] = lnic_read();
    m_data[3] = lnic_read();
    m_data[4] = lnic_read();
    m_data[5] = lnic_read();

    // Read in the per-entry data. This has to use malloc so that it persists.
    if (m.n_entries > 0) {
        m.entries = entry_static_buf;
        if (m.n_entries > kStaticBufSize) {
            m.entries = malloc(sizeof(msg_entry_t)*m.n_entries);
        }
        for (int i = 0; i < m.n_entries; i++) {
            // Read in the entry metadata
            msg_entry_t* current_entry = &m.entries[i];
            uint64_t* entry_data = (uint64_t*)current_entry;
            entry_data[0] = lnic_read();
            entry_data[1] = lnic_read();
            entry_data[2] = lnic_read();
            entry_data[3] = lnic_read();

            // Read in the client request. This also needs to use malloc.
            current_entry->data.buf = get_slot();//malloc(sizeof(client_req_t));
            uint64_t* msg_data = (uint64_t*)current_entry->data.buf;
            msg_data[0] = lnic_read();
            msg_data[1] = lnic_read();
            msg_data[2] = lnic_read();
            msg_data[3] = lnic_read();
            msg_data[4] = lnic_read();
            msg_data[5] = lnic_read();
            msg_data[6] = lnic_read();
            msg_data[7] = lnic_read();
            msg_data[8] = lnic_read();
            msg_data[9] = lnic_read();
        }
    } else {
        m.entries = nullptr;
    }
    lnic_msg_done();
    //printf("starting to process appendentries\n");

    // Process the reassembled request
    uint32_t src_ip = (header & 0xffffffff00000000) >> 32;
    msg_appendentries_response_t msg_response_buf;
    int raft_retval = raft_recv_appendentries(sv->raft, raft_get_node(sv->raft, src_ip), &m, &msg_response_buf);
    assert(raft_retval == 0);
    //printf("starting to send append entries response\n");

    // Send the response to the appendentries request
    // Send the header
    header = 0;
    header |= (uint64_t)src_ip << 32;
    header |= (uint16_t)(sizeof(msg_appendentries_response_t));
    lnic_write_r(header);

    // Send the message
    msg_response_buf.msg_id = ReqType::kAppendEntriesResponse;
    uint64_t* msg_response_data = (uint64_t*)&msg_response_buf;
    lnic_write_r(msg_response_data[0]);
    lnic_write_r(msg_response_data[1]);
    lnic_write_r(msg_response_data[2]);
    lnic_write_r(msg_response_data[3]);
}

void service_append_entries_response(uint64_t header, uint64_t start_word) {
    // Read in the message
    //printf("Receiving append entries response\n");
    msg_appendentries_response_t response;
    uint64_t* response_data = (uint64_t*)&response;
    response_data[0] = start_word;
    response_data[1] = lnic_read();
    response_data[2] = lnic_read();
    response_data[3] = lnic_read();
    lnic_msg_done();
    //printf("finished reading response\n");

    // Process the reassembled message
    uint32_t src_ip = (header & 0xffffffff00000000) >> 32;
    int raft_retval = raft_recv_appendentries_response(sv->raft, raft_get_node(sv->raft, src_ip), &response);
    assert(raft_retval == 0 || raft_retval == RAFT_ERR_NOT_LEADER);
}

void service_pending_messages() {
    //lnic_wait();
    //uint64_t header = lnic_read();
    if (!lnic_ready()) {
        lnic_idle();
        return;
    }
    initial_msg_recv = csr_read(mcycle);
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
    //printf("Starting server main\n");
    raft_init();
    int periodic_counter = 0;

    alloc->base_ptr = malloc(kNumAllocSlots*64*sizeof(client_req_t));
    alloc->current_index = 0;
    for (int i = 0; i < kNumAllocSlots; i++) {
        alloc->used_slots[i] = 0;
    }
    sv->max_pending_requests = kMaxStaticPendingRequests;
    sv->leader_saveinfo.reserve(kMaxStaticPendingRequests);
    for (int i = 0; i < sv->max_pending_requests; i++) {
        sv->leader_saveinfo.push_back(leader_saveinfo_t());
    }

    while (true) {
        if (periodic_counter == 100) {
            periodic_raft_wrapper();
            periodic_counter = 0;
        } else {
            periodic_counter++;
        }
        service_pending_messages();

        raft_node_t* leader_node = raft_get_current_leader_node(sv->raft);
        if (!sent_startup_msg && (leader_node != 0)) {
            sent_startup_msg = true;
            send_startup_msg(0, 0);
        }

        // raft_node_t* leader_node = raft_get_current_leader_node(sv->raft);
        // if (leader_node == nullptr) {
        //     continue;
        // }
        // uint32_t leader_ip = raft_node_get_id(leader_node);
        //printf("Current leader ip is %#x\n", leader_ip);

        // Reply to clients if any entries have committed
        for (int i = 0; i < sv->next_open_leader_sav_index; i++) {
            leader_saveinfo_t &leader_sav = sv->leader_saveinfo[i];
            //printf("Log has %d entries\n", sv->log_record.size());
            if (!leader_sav.in_use) {
                continue;
            }
            //printf("Leader has saved a response\n");
            int commit_status = raft_msg_entry_response_committed(sv->raft, &leader_sav.msg_entry_response);
            assert(commit_status == 0 || commit_status == 1);
            if (commit_status == 1) {
                // We've already committed the entry
                raft_apply_all(sv->raft);
                leader_sav.in_use = false;
                send_client_response(leader_sav.header, leader_sav.sent_time, ClientRespType::kSuccess, sv->own_ip_addr);
                uint64_t end_time = csr_read(mcycle);

                // Lower the save index if this entry is at the end of the allocated entry list
                if (i == sv->next_open_leader_sav_index - 1) {
                    sv->next_open_leader_sav_index--;
                }
                //printf("total elapsed %ld\n", end_time - initial_msg_recv);
            }
        }
    }

    return 0;
}

void stall_start() {
    uint32_t stall_cycles = sv->stall_count * kStallLoopTurnsPerCount;
    for (uint32_t i = 0; i < stall_cycles; i++) {
        asm volatile("nop");
    }
}

int main(int argc, char** argv) {
    // Setup the C++ libraries
    sbrk_init((long int*)data_end);
    atexit(__libc_fini_array);
    __libc_init_array();

    lnic_add_context(0, 1);
    //printf("append entries struct size is: %d\n", sizeof(msg_appendentries_t));
    //printf("response size is: %d\n", sizeof(msg_appendentries_response_t));
    //printf("entry size is: %d\n", sizeof(msg_entry_t));
    //printf("client req size is: %d\n", sizeof(client_req_t));

    // Initialize variables and parse arguments
    //printf("Started raft main\n");
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
    //printf("1\n");
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
            sv->stall_count = i - 3;
            srand(i - 3);
        }
    }
    //printf("2\n");
    sv->num_servers = sv->peer_ip_addrs.size();

    // Determine if client or server. Client will have an ip that is not a member of the peer ip set.
    if (!is_server) {
        // This is a client
        sv->stall_count = sv->num_servers;
        srand(sv->num_servers);
        stall_start();
        return client_main();
    } else {
        // This is a server
        stall_start();
        return server_main();
    }


    return 0;
}