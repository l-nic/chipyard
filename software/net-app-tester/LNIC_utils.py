
from NDP_headers import *
from Homa_headers import *

MAX_SEG_LEN_BYTES = 1024
RTT_PKTS = 5

def compute_num_pkts(msg_len):
    return msg_len/MAX_SEG_LEN_BYTES if (msg_len % MAX_SEG_LEN_BYTES == 0) else msg_len/MAX_SEG_LEN_BYTES + 1

class NDPReceiver(object):
    """
    Receive NDP DATA pkts and send NDP ACK+PULL pkts.
    """
    def __init__(self, iface, prn=None):
        # iface to send packets on
        self.iface = iface

        # map {dst_ip, dst_port, src_ip, src_port, tx_msg_id => ["pkt_0_data", ..., "pkt_N_data"]}
        self.buffers = {}
        # bitmap to determine when all pkts have arrived, {dst_ip, dst_port, src_ip, src_port, tx_msg_id => bitmap}
        self.received = {}

        # optional custom callback function to invoke for each processed packet
        self.prn = prn

        # store list of all received msgs: [((dst_ip, dst_port, src_ip, src_port, tx_msg_id), msg), ...]
        self.msgs = []

    def process_pkt(self, p):
        if p.haslayer(NDP) and p[NDP].flags.DATA:
            if len(p)-14 != p[IP].len:
                print "ERROR: len(p) = {}, p[IP].len = {}".format(len(p), p[IP].len)
            msg_key = (p[IP].dst, p[NDP].dst_context, p[IP].src, p[NDP].src_context, p[NDP].tx_msg_id)
            offset = p[NDP].pkt_offset
            num_pkts = compute_num_pkts(p[NDP].msg_len)
            # assemble msgs
            if msg_key in self.buffers:
                self.buffers[msg_key][offset] = str(p[NDP].payload)
                self.received[msg_key] |= (1<<offset)
            else:
                self.buffers[msg_key] = ["" for i in range(num_pkts)]
                self.buffers[msg_key][offset] = str(p[NDP].payload)
                self.received[msg_key] = (1<<offset)
            # check if all pkts received
            if self.received[msg_key] == (1<<num_pkts)-1:
                self.msgs.append((msg_key, ''.join(self.buffers[msg_key])))
                del self.buffers[msg_key]
                del self.received[msg_key]
            # send ACK+PULL
            pull_offset = p[NDP].pkt_offset + RTT_PKTS
            ack_pull = Ether(dst=p[Ether].src, src=p[Ether].dst) / \
                       IP(dst=p[IP].src, src=p[IP].dst) / \
                       NDP(flags="ACK+PULL",
                            src_context=p[NDP].dst_context,
                            dst_context=p[NDP].src_context,
                            msg_len=p[NDP].msg_len,
                            pkt_offset=p[NDP].pkt_offset,
                            pull_offset=pull_offset,
                            tx_msg_id=p[NDP].tx_msg_id,
                            buf_ptr=p[NDP].buf_ptr,
                            buf_size_class=p[NDP].buf_size_class) / \
                       Raw("\x00"*64)
            print "Sending ACK+PULL: {}".format(ack_pull.summary())
            sendp(ack_pull, iface=self.iface)
        if self.prn:
            self.prn(p)

class HomaReceiver(object):
    """
    Receive Homa DATA pkts and send Homa ACK and GRANT pkts.
    """
    def __init__(self, iface, prn=None):
        # iface to send packets on
        self.iface = iface

        # map {dst_ip, dst_port, src_ip, src_port, tx_msg_id => ["pkt_0_data", ..., "pkt_N_data"]}
        self.buffers = {}
        # bitmap to determine when all pkts have arrived, {dst_ip, dst_port, src_ip, src_port, tx_msg_id => bitmap}
        self.received = {}

        # optional custom callback function to invoke for each processed packet
        self.prn = prn

        # store list of all received msgs: [((dst_ip, dst_port, src_ip, src_port, tx_msg_id), msg), ...]
        self.msgs = []

    def process_pkt(self, p):
        if p.haslayer(Homa) and p[Homa].flags.DATA:
            if len(p)-14 != p[IP].len:
                print "ERROR: len(p) = {}, p[IP].len = {}".format(len(p), p[IP].len)
            msg_key = (p[IP].dst, p[Homa].dst_context, p[IP].src, p[Homa].src_context, p[Homa].tx_msg_id)
            offset = p[Homa].pkt_offset
            num_pkts = compute_num_pkts(p[Homa].msg_len)
            # assemble msgs
            if msg_key in self.buffers:
                self.buffers[msg_key][offset] = str(p[Homa].payload)
                self.received[msg_key] |= (1<<offset)
            else:
                self.buffers[msg_key] = ["" for i in range(num_pkts)]
                self.buffers[msg_key][offset] = str(p[Homa].payload)
                self.received[msg_key] = (1<<offset)
            # check if all pkts received
            if self.received[msg_key] == (1<<num_pkts)-1:
                self.msgs.append((msg_key, ''.join(self.buffers[msg_key])))
                del self.buffers[msg_key]
                del self.received[msg_key]
            # send ACK and GRANT pkts
            ack = Ether(dst=p[Ether].src, src=p[Ether].dst) / \
                  IP(dst=p[IP].src, src=p[IP].dst) / \
                  Homa(flags="ACK",
                       src_context=p[Homa].dst_context,
                       dst_context=p[Homa].src_context,
                       msg_len=p[Homa].msg_len,
                       pkt_offset=p[Homa].pkt_offset,
                       grant_offset=0, # unused
                       grant_prio=0,   # unused
                       tx_msg_id=p[Homa].tx_msg_id,
                       buf_ptr=p[Homa].buf_ptr,
                       buf_size_class=p[Homa].buf_size_class) / \
                  Raw("\x00")
            grant_offset = p[Homa].pkt_offset + RTT_PKTS
            grant = Ether(dst=p[Ether].src, src=p[Ether].dst) / \
                    IP(dst=p[IP].src, src=p[IP].dst) / \
                    Homa(flags="GRANT",
                         src_context=p[Homa].dst_context,
                         dst_context=p[Homa].src_context,
                         msg_len=p[Homa].msg_len,
                         pkt_offset=p[Homa].pkt_offset,
                         grant_offset=grant_offset,
                         grant_prio=0,
                         tx_msg_id=p[Homa].tx_msg_id,
                         buf_ptr=p[Homa].buf_ptr,
                         buf_size_class=p[Homa].buf_size_class) / \
                    Raw("\x00")
            print "Sending ACK: {}".format(ack.summary())
            print "Sending GRANT: {}".format(grant.summary())
            sendp([ack, grant], iface=self.iface)
        if self.prn:
            self.prn(p)

