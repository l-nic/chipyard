// ----------------------------------------------------------------------- //
// lnic.p4
// ----------------------------------------------------------------------- //

#define MAX_SEG_LEN_BYTES 1024
#define L2_MAX_SEG_LEN_BYTES 10
#define MAX_SEGS_PER_MSG 38

typedef bit<48>  EthAddr_t;
typedef bit<32>  IPv4Addr_t;
typedef bit<16>  ContextID_t;
typedef bit<16>  MsgID_t;
typedef bit<MAX_SEGS_PER_MSG>  PktBitmap_t;

// ******************************************************************************* //
// *************************** C O N S T A N T S ********************************* //
// ******************************************************************************* //

// TODO(sibanez): probably need to update these ...

const bit<16> IP_HDR_BYTES = 20;

const bit<16> LNIC_HDR_BYTES = 30;
const bit<16> LNIC_CTRL_PKT_BYTES = 31;

const bit<16> IPV4_TYPE = 0x0800;

// TODO(sarslan): Can we make these runtime configurable 
//                parameters unique to protocol implementations
const bit<8> HOMA_NUM_UNSCHEDULED_PRIOS = 1;
const bit<8> HOMA_NUM_TOTAL_PRIOS       = 4;
const bit<8> HOMA_OVERCOMMITMENT_LEVEL  = 3;

// ******************************************************************************* //
// *************************** M E T A D A T A *********************************** //
// ******************************************************************************* //

// We'll pass runtime configurable parameters in as packet metadata.
struct parameters_t {
    EthAddr_t switch_mac_addr;
    EthAddr_t nic_mac_addr;
    IPv4Addr_t nic_ip_addr;
    bit<16> rtt_pkts;
}

// Metadata that is passed to the Assembly module.
struct ingress_metadata_t {
    IPv4Addr_t   src_ip;
    ContextID_t  src_context;
    bit<16>      msg_len;
    bit<8>       pkt_offset;
    ContextID_t  dst_context;
    MsgID_t      rx_msg_id;
    MsgID_t      tx_msg_id;
    bool         is_last_pkt;
}

struct full_ingress_metadata_t {
    parameters_t params;
    ingress_metadata_t meta;
}

// Metadata that is passed to the Egress pipeline.
struct egress_metadata_t {
    IPv4Addr_t  dst_ip;
    ContextID_t dst_context;
    bit<16>     msg_len;
    bit<8>      pkt_offset;
    ContextID_t src_context;
    MsgID_t     tx_msg_id;
    bit<16>     buf_ptr;
    bit<8>      buf_size_class;
    bit<16>     credit;
    bit<8>      rank;
    bit<8>      flags;
    bool        is_new_msg;
    bool        is_rtx;
}

struct full_egress_metadata_t {
    parameters_t params;
    egress_metadata_t meta;
}

// ****************************************************************************** //
// *************************** H E A D E R S  *********************************** //
// ****************************************************************************** //

// standard Ethernet header (14 bytes = 112 bits)
header eth_mac_t {
    EthAddr_t dstAddr;
    EthAddr_t srcAddr;
    bit<16> etherType;
}

// IPv4 header without options (20 bytes = 160 bits)
header ipv4_t {
    bit<4> version;
    bit<4> ihl;
    bit<8> tos;
    bit<16> totalLen;
    bit<16> identification;
    bit<3> flags;
    bit<13> fragOffset;
    bit<8> ttl;
    bit<8> protocol;
    bit<16> hdrChecksum;
    IPv4Addr_t srcAddr;
    IPv4Addr_t dstAddr;
}

// ****************************************************************************** //
// ********************** COMMON EXTERN INTERFACES ****************************** //
// ****************************************************************************** //

/* IO for get_rx_msg_info extern */
// Input
struct get_rx_msg_info_req_t {
    bool        mark_received;
    IPv4Addr_t  src_ip;
    ContextID_t src_context;
    MsgID_t     tx_msg_id;
    bit<16>     msg_len;
    bit<8>      pkt_offset;
}
// Output
struct get_rx_msg_info_resp_t {
    bool fail;
    MsgID_t rx_msg_id;
    bool is_new_msg;
    bool is_new_pkt;
    bool is_last_pkt;
    bit<9> ackNo;
}

/* IO for delivered event */
struct delivered_meta_t {
  MsgID_t tx_msg_id;
  PktBitmap_t delivered_pkts;
  bit<16> msg_len;
  bit<16> buf_ptr;
  bit<8> buf_size_class;
}

/* IO for creditToBtx event */
struct creditToBtx_meta_t {
  MsgID_t tx_msg_id;
  bool rtx;
  bit<8> rtx_pkt_offset;
  bool update_credit;
  bit<16> new_credit;
  // Additional fields for generating pkts
  // NOTE: these could be stored in tables indexed by tx_msg_id, but this would require extra state ...
  bit<16> buf_ptr;
  bit<8> buf_size_class;
  IPv4Addr_t dst_ip;
  ContextID_t dst_context;
  bit<16> msg_len;
  ContextID_t src_context;
}

// dummy response for events
struct dummy_t {
  bit<1> unused;
}

