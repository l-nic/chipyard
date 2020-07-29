#include <functional>
#include <queue>
#include <algorithm>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <unistd.h>
#include <omp.h>
#include <cstdlib>
#include <arpa/inet.h>
#include <string>
#include <sstream>

#include <time.h>
#include <netinet/ether.h>
#include <net/ethernet.h>
#include "Packet.h"
#include "EthLayer.h"
#include "IPv4Layer.h"
#include "LnicLayer.h"
#include "AppLayer.h"

#define IGNORE_PRINTF

#ifdef IGNORE_PRINTF
#define printf(fmt, ...) (0)
#endif

// param: link latency in cycles
// assuming 3.2 GHz, this number / 3.2 = link latency in ns
// e.g. setting this to 35000 gives you 35000/3.2 = 10937.5 ns latency
// IMPORTANT: this must be a multiple of 7
//
// THIS IS SET BY A COMMAND LINE ARGUMENT. DO NOT CHANGE IT HERE.
//#define LINKLATENCY 6405
int LINKLATENCY = 0;

// param: switching latency in cycles
// assuming 3.2 GHz, this number / 3.2 = switching latency in ns
//
// THIS IS SET BY A COMMAND LINE ARGUMENT. DO NOT CHANGE IT HERE.
int switchlat = 0;

#define SWITCHLATENCY (switchlat)

// param: numerator and denominator of bandwidth throttle
// Used to throttle outbound bandwidth from port
//
// THESE ARE SET BY A COMMAND LINE ARGUMENT. DO NOT CHANGE IT HERE.
int throttle_numer = 1;
int throttle_denom = 1;

// uncomment to use a limited output buffer size, OUTPUT_BUF_SIZE
//#define LIMITED_BUFSIZE

// size of output buffers, in # of flits
// only if LIMITED BUFSIZE is set
// TODO: expose in manager
#define OUTPUT_BUF_SIZE (131072L)

// pull in # clients config
#define NUMCLIENTSCONFIG
#include "switchconfig.h"
#undef NUMCLIENTSCONFIG

// DO NOT TOUCH
#define NUM_TOKENS (LINKLATENCY)
#define TOKENS_PER_BIGTOKEN (7)
#define BIGTOKEN_BYTES (64)
#define NUM_BIGTOKENS (NUM_TOKENS/TOKENS_PER_BIGTOKEN)
#define BUFSIZE_BYTES (NUM_BIGTOKENS*BIGTOKEN_BYTES)

// DO NOT TOUCH
#define SWITCHLAT_NUM_TOKENS (SWITCHLATENCY)
#define SWITCHLAT_NUM_BIGTOKENS (SWITCHLAT_NUM_TOKENS/TOKENS_PER_BIGTOKEN)
#define SWITCHLAT_BUFSIZE_BYTES (SWITCHLAT_NUM_BIGTOKENS*BIGTOKEN_BYTES)

uint64_t this_iter_cycles_start = 0;

// pull in mac2port array
#define MACPORTSCONFIG
#include "switchconfig.h"
#undef MACPORTSCONFIG

#include "flit.h"
#include "baseport.h"
#include "shmemport.h"
#include "socketport.h"
#include "sshport.h"

#define ETHER_HEADER_SIZE          14
#define IP_DST_FIELD_OFFSET        16 // Dest field immediately after, in same 64-bit flit
#define IP_SUBNET_OFFSET           2
#define IP_HEADER_SIZE             20 // TODO: Not always, just currently the case with L-NIC.

#define LNIC_HEADER_MSG_LEN_OFFSET 5
#define LNIC_HEADER_PKT_OFFSET_OFFSET 7
#define LNIC_HEADER_PULL_OFFSET_OFFSET 8
#define LNIC_PACKET_CHOPPED_SIZE   128 // Bytes, the minimum L-NIC packet size
#define LNIC_HEADER_SIZE           30

#define APP_HEADER_SIZE 16

#define QUEUE_SIZE_LOG_INTERVAL 100 // 100 cycles between log interval points
#define LOG_QUEUE_SIZE
#define LOG_EVENTS
#define LOG_ALL_PACKETS

// These are both set by command-line arguments. Don't change them here.
int HIGH_PRIORITY_OBUF_SIZE = 0;
int LOW_PRIORITY_OBUF_SIZE = 0;

// These are all set by command-line arguments. Don't change them here.
char* DISTRIBUTION_TYPE;
char* TEST_TYPE;
int POISSON_LAMBDA = 0;
#define RTT_PKTS 2 // TODO: This should be user-configurable

class parsed_packet_t {
 private:
    pcpp::Packet* pcpp_packet;
 public:
    pcpp::EthLayer* eth;
    pcpp::IPv4Layer* ip;
    pcpp::LnicLayer* lnic;
    pcpp::AppLayer* app;
    switchpacket* tsp;

    parsed_packet_t() {
        eth = nullptr;
        ip = nullptr;
        lnic = nullptr;
        app = nullptr;
        tsp = nullptr;
        pcpp_packet = nullptr;
    }

    ~parsed_packet_t() {
        if (pcpp_packet != nullptr) {
            delete pcpp_packet;
        }
    }

    bool parse(switchpacket* tsp) {
        uint64_t packet_size_bytes = tsp->amtwritten * sizeof(uint64_t);
        struct timeval format_time;
        format_time.tv_sec = tsp->timestamp / 1000000000;
        format_time.tv_usec = (tsp->timestamp % 1000000000) / 1000;
        pcpp::RawPacket raw_packet((const uint8_t*)tsp->dat, 200*sizeof(uint64_t), format_time, false, pcpp::LINKTYPE_ETHERNET);
        pcpp::Packet* parsed_packet = new pcpp::Packet(&raw_packet);
        pcpp::EthLayer* eth_layer = parsed_packet->getLayerOfType<pcpp::EthLayer>();
        pcpp::IPv4Layer* ip_layer = parsed_packet->getLayerOfType<pcpp::IPv4Layer>();
        pcpp::LnicLayer* lnic_layer = (pcpp::LnicLayer*)parsed_packet->getLayerOfType(pcpp::LNIC, 0);
        pcpp::AppLayer* app_layer = (pcpp::AppLayer*)parsed_packet->getLayerOfType(pcpp::GenericPayload, 0);
        if (!eth_layer || !ip_layer || !lnic_layer || !app_layer) {
            if (!eth_layer) fprintf(stdout, "Null eth layer\n");
            if (!ip_layer) fprintf(stdout, "Null ip layer\n");
            if (!lnic_layer) fprintf(stdout, "Null lnic layer\n");
            if (!app_layer) fprintf(stdout, "Null app layer\n");
            this->eth = nullptr;
            this->ip = nullptr;
            this->lnic = nullptr;
            this->app = nullptr;
            this->tsp = nullptr;
            delete parsed_packet;
            this->pcpp_packet = nullptr;
            return false;
        }
        this->eth = eth_layer;
        this->ip = ip_layer;
        this->lnic = lnic_layer;
        this->app = app_layer;
        this->tsp = tsp;
        this->pcpp_packet = parsed_packet;
        return true;
    }
};

// TODO: replace these port mapping hacks with a mac -> port mapping,
// could be hardcoded

BasePort * ports[NUMPORTS];
void send_with_priority(uint16_t port, switchpacket* tsp);
void handle_packet(switchpacket* tsp);
void generate_load_packets();

/* switch from input ports to output ports */
void do_fast_switching() {
#pragma omp parallel for
    for (int port = 0; port < NUMPORTS; port++) {
        ports[port]->setup_send_buf();
    }


// preprocess from raw input port to packets
#pragma omp parallel for
for (int port = 0; port < NUMPORTS; port++) {
    BasePort * current_port = ports[port];
    uint8_t * input_port_buf = current_port->current_input_buf;

    for (int tokenno = 0; tokenno < NUM_TOKENS; tokenno++) {
        if (is_valid_flit(input_port_buf, tokenno)) {
            uint64_t flit = get_flit(input_port_buf, tokenno);

            switchpacket * sp;
            if (!(current_port->input_in_progress)) {
                sp = (switchpacket*)calloc(sizeof(switchpacket), 1);
                current_port->input_in_progress = sp;

                // here is where we inject switching latency. this is min port-to-port latency
                sp->timestamp = this_iter_cycles_start + tokenno + SWITCHLATENCY;
                sp->sender = port;
            }
            sp = current_port->input_in_progress;

            sp->dat[sp->amtwritten++] = flit;
            if (is_last_flit(input_port_buf, tokenno)) {
                current_port->input_in_progress = NULL;
                if (current_port->push_input(sp)) {
                    printf("packet timestamp: %ld, len: %ld, sender: %d\n",
                            this_iter_cycles_start + tokenno,
                            sp->amtwritten, port);
                }
            }
        }
    }
}

// next do the switching. but this switching is just shuffling pointers,
// so it should be fast. it has to be serial though...

// NO PARALLEL!
// shift pointers to output queues, but in order. basically.
// until the input queues have no more complete packets
// 1) find the next switchpacket with the lowest timestamp across all the inputports
// 2) look at its mac, copy it into the right ports
//          i) if it's a broadcast: sorry, you have to make N-1 copies of it...
//          to put into the other queues

struct tspacket {
    uint64_t timestamp;
    switchpacket * switchpack;

    bool operator<(const tspacket &o) const
    {
        return timestamp > o.timestamp;
    }
};

typedef struct tspacket tspacket;


// TODO thread safe priority queue? could do in parallel?
std::priority_queue<tspacket> pqueue;

for (int i = 0; i < NUMPORTS; i++) {
    while (!(ports[i]->inputqueue.empty())) {
        switchpacket * sp = ports[i]->inputqueue.front();
        ports[i]->inputqueue.pop();
        pqueue.push( tspacket { sp->timestamp, sp });
    }
}

while (!pqueue.empty()) {
    switchpacket * tsp = pqueue.top().switchpack;
    pqueue.pop();
    // Instead of switching the input packets, the load generator will log any inputs
    // and independently generate sets of outputs.

    // Send ACKs and PULLs and log non load-gen packets
    handle_packet(tsp);
    free(tsp);
}

// Generate and log load packets
generate_load_packets();

// Log queue sizes if logging is enabled
#ifdef LOG_QUEUE_SIZE
if (this_iter_cycles_start % QUEUE_SIZE_LOG_INTERVAL == 0) {
    bool non_zero_buffer = false;
    for (int i = 0; i < NUMPORTS; i++) {
        if (ports[i]->outputqueue_high_size != 0 || ports[i]->outputqueue_low_size != 0) {
            non_zero_buffer = true;
            break;
        }
    }
    if (non_zero_buffer) {
        fprintf(stdout, "&&CSV&&QueueSize,%ld", this_iter_cycles_start);
        for (int i = 0; i < NUMPORTS; i++) {
            fprintf(stdout, ",%d,%ld,%ld", i, ports[i]->outputqueue_high_size, ports[i]->outputqueue_low_size);
        }
        fprintf(stdout, "\n");
    }
}
#endif

// finally in parallel, flush whatever we can to the output queues based on timestamp

#pragma omp parallel for
for (int port = 0; port < NUMPORTS; port++) {
    BasePort * thisport = ports[port];
    thisport->write_flits_to_output();
}

}

void print_packet(char* direction, parsed_packet_t* packet) {
    fprintf(stdout, "%s IP(src=%s, dst=%s), %s, %s, packet_len=%d\n", direction,
            packet->ip->getSrcIpAddress().toString().c_str(), packet->ip->getDstIpAddress().toString().c_str(),
            packet->lnic->toString().c_str(), packet->app->toString().c_str(), packet->tsp->amtwritten * sizeof(uint64_t));
}

void handle_packet(switchpacket* tsp) {
    // Parse and log the incoming packet
    parsed_packet_t packet;
    bool is_valid = packet.parse(tsp);
    if (!is_valid) {
        fprintf(stdout, "Invalid received packet.\n");
        return;
    }
    print_packet("RECV", &packet);

    // Send ACK+PULL responses to DATA packets
    if (packet.lnic->getLnicHeader()->flags & LNIC_DATA_FLAG_MASK) {
        // Calculate the ACK+PULL values
        pcpp::lnichdr* lnic_hdr = packet.lnic->getLnicHeader();
        uint16_t pull_offset = lnic_hdr->pkt_offset + RTT_PKTS;
        uint8_t flags = LNIC_ACK_FLAG_MASK | LNIC_PULL_FLAG_MASK;
        uint64_t ack_packet_size_bytes = ETHER_HEADER_SIZE + IP_HEADER_SIZE + LNIC_HEADER_SIZE + APP_HEADER_SIZE;

        // Build the new packet layers
        pcpp::EthLayer new_eth_layer(packet.eth->getDestMac(), packet.eth->getSourceMac());
        pcpp::IPv4Layer new_ip_layer(packet.ip->getDstIpAddress(), packet.ip->getSrcIpAddress());
        new_ip_layer.getIPv4Header()->ipId = htons(1);
        new_ip_layer.getIPv4Header()->timeToLive = 64;
        new_ip_layer.getIPv4Header()->protocol = 153; // Protocol code for LNIC
        pcpp::LnicLayer new_lnic_layer(flags, ntohs(lnic_hdr->dst_context), ntohs(lnic_hdr->src_context),
                                       ntohs(lnic_hdr->msg_len), lnic_hdr->pkt_offset, pull_offset,
                                       ntohs(lnic_hdr->tx_msg_id), ntohs(lnic_hdr->buf_ptr), lnic_hdr->buf_size_class);
        pcpp::AppLayer new_app_layer(3, 4);

        // Join the layers into a new packet
        pcpp::Packet new_packet(ack_packet_size_bytes);
        new_packet.addLayer(&new_eth_layer);
        new_packet.addLayer(&new_ip_layer);
        new_packet.addLayer(&new_lnic_layer);
        new_packet.addLayer(&new_app_layer);
        new_packet.computeCalculateFields();

        // Convert the packet to a switchpacket
        switchpacket* new_tsp = (switchpacket*)calloc(sizeof(switchpacket), 1);
        new_tsp->timestamp = this_iter_cycles_start;
        new_tsp->amtwritten = ack_packet_size_bytes / sizeof(uint64_t);
        new_tsp->amtread = 0;
        new_tsp->sender = 0;
        memcpy(new_tsp->dat, new_packet.getRawPacket()->getRawData(), ack_packet_size_bytes);

        // Verify and log the switchpacket
        // TODO: For now we only work with port 0.
        parsed_packet_t sent_packet;
        if (!sent_packet.parse(new_tsp)) {
            fprintf(stdout, "Invalid sent packet.\n");
            free(new_tsp);
            return;
        }
        print_packet("SEND", &sent_packet);
        send_with_priority(0, new_tsp);
    }
}

void generate_load_packets() {

}

void send_with_priority(uint16_t port, switchpacket* tsp) {
    uint8_t lnic_header_flags = *((uint8_t*)tsp->dat + ETHER_HEADER_SIZE + IP_HEADER_SIZE);
    bool is_data = lnic_header_flags & LNIC_DATA_FLAG_MASK;
    bool is_chop = lnic_header_flags & LNIC_CHOP_FLAG_MASK;
    uint64_t packet_size_bytes = tsp->amtwritten * sizeof(uint64_t);

    if (is_data && !is_chop) {
        // Regular data, send to low priority queue or chop and send to high priority
        // queue if low priority queue is full.
        if (packet_size_bytes + ports[port]->outputqueue_low_size < LOW_PRIORITY_OBUF_SIZE) {
            ports[port]->outputqueue_low.push(tsp);
            ports[port]->outputqueue_low_size += packet_size_bytes;
        } else {
            // Try to chop the packet
            if (LNIC_PACKET_CHOPPED_SIZE + ports[port]->outputqueue_high_size < HIGH_PRIORITY_OBUF_SIZE) {
#ifdef LOG_EVENTS
                fprintf(stdout, "&&CSV&&Events,Chopped,%ld,%d\n", this_iter_cycles_start, port);
#endif
                switchpacket * tsp2 = (switchpacket*)calloc(sizeof(switchpacket), 1);
                tsp2->timestamp = tsp->timestamp;
                tsp2->amtwritten = LNIC_PACKET_CHOPPED_SIZE / sizeof(uint64_t);
                tsp2->amtread = tsp->amtread;
                tsp2->sender = tsp->sender;
                memcpy(tsp2->dat, tsp->dat, ETHER_HEADER_SIZE + IP_HEADER_SIZE + LNIC_HEADER_SIZE);
                uint64_t lnic_flag_offset = (uint64_t)tsp2->dat + ETHER_HEADER_SIZE + IP_HEADER_SIZE;
                *(uint8_t*)(lnic_flag_offset) |= LNIC_CHOP_FLAG_MASK;
                free(tsp);
                ports[port]->outputqueue_high.push(tsp2);
                ports[port]->outputqueue_high_size += LNIC_PACKET_CHOPPED_SIZE;

            } else {
                // TODO: We should really drop the lowest priority packet sometimes, not always the newly arrived packet
#ifdef LOG_EVENTS
                fprintf(stdout, "&&CSV&&Events,DroppedBothFull,%ld,%d\n", this_iter_cycles_start, port);
#endif
                free(tsp);
            }
        }
    } else if ((is_data && is_chop) || (!is_data && !is_chop)) {
        // Chopped data or control, send to high priority output queue
        if (packet_size_bytes + ports[port]->outputqueue_high_size < HIGH_PRIORITY_OBUF_SIZE) {
            ports[port]->outputqueue_high.push(tsp);
            ports[port]->outputqueue_high_size += packet_size_bytes;
        } else {
#ifdef LOG_EVENTS
            fprintf(stdout, "&&CSV&&Events,DroppedControlFull,%ld,%d\n", this_iter_cycles_start, port);
#endif
            free(tsp);
        }
    } else {
        fprintf(stdout, "Invalid combination: Chopped control packet. Dropping.\n");
        free(tsp);
        // Chopped control packet. This shouldn't be possible.
    }
}

static void simplify_frac(int n, int d, int *nn, int *dd)
{
    int a = n, b = d;

    // compute GCD
    while (b > 0) {
        int t = b;
        b = a % b;
        a = t;
    }

    *nn = n / a;
    *dd = d / a;
}

int main (int argc, char *argv[]) {
    int bandwidth;

    if (argc < 9) {
        // if insufficient args, error out
        fprintf(stdout, "usage: ./switch LINKLATENCY SWITCHLATENCY BANDWIDTH HIGH_PRIORITY_OBUF_SIZE LOW_PRIORITY_OBUF_SIZE DISTRIBUTION_TYPE TEST_TYPE POISSON_LAMBDA\n");
        fprintf(stdout, "insufficient args provided\n.");
        fprintf(stdout, "LINKLATENCY and SWITCHLATENCY should be provided in cycles.\n");
        fprintf(stdout, "BANDWIDTH should be provided in Gbps\n");
        fprintf(stdout, "OBUF SIZES should be provided in bytes.\n");
        fprintf(stdout, "DISTRIBUTION_TYPE should be one of FIXED, EXPONENTIAL, BIMODAL\n");
        fprintf(stdout, "TEST_TYPE should be one of ONE_CONTEXT_FOUR_CORES, FOUR_CONTEXTS_FOUR_CORES, TWO_CONTEXTS_FOUR_SHARED_CORES, \
                          DIF_PRIORITY_LNIC_DRIVEN, DIF_PRIORITY_TIMER_DRIVEN, HIGH_PRIORITY_C1_STALL, \
                          LOW_PRIORITY_C1_STALL\n");
        fprintf(stdout, "POISSON_LAMBDA should be provided in mean cycles between generated requests\n");
        exit(1);
    }

    LINKLATENCY = atoi(argv[1]);
    switchlat = atoi(argv[2]);
    bandwidth = atoi(argv[3]);
    HIGH_PRIORITY_OBUF_SIZE = atoi(argv[4]);
    LOW_PRIORITY_OBUF_SIZE = atoi(argv[5]);
    DISTRIBUTION_TYPE = argv[6];
    TEST_TYPE = argv[7];
    POISSON_LAMBDA = atoi(argv[8]);

    simplify_frac(bandwidth, 200, &throttle_numer, &throttle_denom);

    fprintf(stdout, "Using link latency: %d\n", LINKLATENCY);
    fprintf(stdout, "Using switching latency: %d\n", SWITCHLATENCY);
    fprintf(stdout, "BW throttle set to %d/%d\n", throttle_numer, throttle_denom);
    fprintf(stdout, "High priority obuf size: %d\n", HIGH_PRIORITY_OBUF_SIZE);
    fprintf(stdout, "Low priority obuf size: %d\n", LOW_PRIORITY_OBUF_SIZE);

    if ((LINKLATENCY % 7) != 0) {
        // if invalid link latency, error out.
        fprintf(stdout, "INVALID LINKLATENCY. Currently must be multiple of 7 cycles.\n");
        exit(1);
    }

    omp_set_num_threads(NUMPORTS); // we parallelize over ports, so max threads = # ports

#define PORTSETUPCONFIG
#include "switchconfig.h"
#undef PORTSETUPCONFIG

    while (true) {

        // handle sends
#pragma omp parallel for
        for (int port = 0; port < NUMPORTS; port++) {
            ports[port]->send();
        }

        // handle receives. these are blocking per port
#pragma omp parallel for
        for (int port = 0; port < NUMPORTS; port++) {
            ports[port]->recv();
        }
 
#pragma omp parallel for
        for (int port = 0; port < NUMPORTS; port++) {
            ports[port]->tick_pre();
        }

        do_fast_switching();

        this_iter_cycles_start += LINKLATENCY; // keep track of time

        // some ports need to handle extra stuff after each iteration
        // e.g. shmem ports swapping shared buffers
#pragma omp parallel for
        for (int port = 0; port < NUMPORTS; port++) {
            ports[port]->tick();
        }

    }
}
