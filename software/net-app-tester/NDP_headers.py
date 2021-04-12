
from scapy.all import *
import struct

NDP_PROTO = 155

class NDP(Packet):
    name = "NDP"
    fields_desc = [
        FlagsField("flags", 0, 8, ["DATA", "ACK", "NACK", "PULL",
                                   "CHOP", "F1", "F2", "F3"]),
        ShortField("src_context", 0),
        ShortField("dst_context", 0),
        ShortField("msg_len", 0),
        ByteField("pkt_offset", 0),
        ShortField("pull_offset", 0),
        ShortField("tx_msg_id", 0),
        ShortField("buf_ptr", 0),
        ByteField("buf_size_class", 0),
        BitField("padding", 0, 15*8) # add 15B of padding to make HDL parsing / deparsing easier ... don't need this if using SDNet ...
    ]
    def mysummary(self):
        return self.sprintf("flags=%flags%, src=%src_context%, dst=%dst_context%, msg_len=%msg_len%, pkt_offset=%pkt_offset%, pull_offset=%pull_offset%, tx_msg_id=%tx_msg_id%")

bind_layers(IP, NDP, proto=NDP_PROTO)

