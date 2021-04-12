
from scapy.all import *
import struct

HOMA_PROTO = 154

class Homa(Packet):
    name = "Homa"
    fields_desc = [
        FlagsField("flags", 0, 8, ["DATA", "ACK", "NACK", "GRANT",
                                   "CHOP", "F1", "F2", "F3"]),
        ShortField("src_context", 0),
        ShortField("dst_context", 0),
        ShortField("msg_len", 0),
        ByteField("pkt_offset", 0),
        ShortField("grant_offset", 0),
        ByteField("grant_prio", 0),
        ShortField("tx_msg_id", 0),
        ShortField("buf_ptr", 0),
        ByteField("buf_size_class", 0),
        BitField("padding", 0, 14*8) # add 15B of padding to make HDL parsing / deparsing easier ... don't need this if using SDNet ...
    ]
    def mysummary(self):
        return self.sprintf("flags=%flags%, src=%src_context%, dst=%dst_context%, pkt_offset=%pkt_offset%, grant_offset=%grant_offset%, grant_prio=%grant_prio%, tx_msg_id=%tx_msg_id%")

bind_layers(IP, Homa, proto=HOMA_PROTO)

