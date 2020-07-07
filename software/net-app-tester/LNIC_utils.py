
from LNIC_headers import *

MAX_SEG_LEN_BYTES = 512
RTT_PKTS = 5

def compute_num_pkts(msg_len):
    return msg_len/MAX_SEG_LEN_BYTES if (msg_len % MAX_SEG_LEN_BYTES == 0) else msg_len/MAX_SEG_LEN_BYTES + 1

class LNICReceiver(object):
    """
    Receive pkts and send ACK+PULL pkts.
    """
    def __init__(self, iface, mac, ip, context):
        # iface to send packets on
        self.iface = iface

        self.mac = mac
        self.ip = ip
        self.context = context

        # map {src_ip, src_port, tx_msg_id => ["pkt_0_data", ..., "pkt_N_data"]}
        self.buffers = {}
        # bitmap to determine when all pkts have arrived, {src_ip, src_port, tx_msg_id => bitmap}
        self.received = {}

        # store list of all received msgs: [((src_ip, src_port, tx_msg_id), msg), ...]
        self.msgs = []

    def process_pkt(self, p):
        if p.haslayer(LNIC) and p[LNIC].flags.DATA and p[LNIC].dst_context == self.context:
            msg_key = (p[IP].src, p[LNIC].src_context, p[LNIC].tx_msg_id)
            offset = p[LNIC].pkt_offset
            num_pkts = compute_num_pkts(p[LNIC].msg_len)
            # assemble msgs
            if msg_key in self.buffers:
                self.buffers[msg_key][offset] = str(p[LNIC].payload)
                self.received[msg_key] |= (1<<offset)
            else:
                self.buffers[msg_key] = ["" for i in range(num_pkts)]
                self.buffers[msg_key][offset] = str(p[LNIC].payload)
                self.received[msg_key] = (1<<offset)
            # check if all pkts received
            if self.received[msg_key] == (1<<num_pkts)-1:
                self.msgs.append((msg_key, ''.join(self.buffers[msg_key])))
                del self.buffers[msg_key]
                del self.received[msg_key]
            # send ACK+PULL
            pull_offset = p[LNIC].pkt_offset + RTT_PKTS
            ack_pull = Ether(dst=p[Ether].src, src=self.mac) / \
                       IP(dst=p[IP].src, src=self.ip) / \
                       LNIC(flags="ACK+PULL",
                            src_context=self.context,
                            dst_context=p[LNIC].src_context,
                            msg_len=p[LNIC].msg_len,
                            pkt_offset=p[LNIC].pkt_offset,
                            pull_offset=pull_offset,
                            tx_msg_id=p[LNIC].tx_msg_id,
                            buf_ptr=p[LNIC].buf_ptr,
                            buf_size_class=p[LNIC].buf_size_class) / \
                       Raw("\x00"*64)
            print "Sending ACK+PULL: tx_msg_id={}, pkt_offset={}, pull_offset={}".format(ack_pull[LNIC].tx_msg_id, ack_pull[LNIC].pkt_offset, ack_pull[LNIC].pull_offset)
            sendp(ack_pull, iface=self.iface)

