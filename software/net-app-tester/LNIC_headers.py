
from scapy.all import *
import struct

LNIC_PROTO = 0x99

class LNIC(Packet):
    name = "LNIC"
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
        ByteField("buf_size_class", 0)
    ]
    def mysummary(self):
        return self.sprintf("flags=%flags%, msg_len=%msg_len%, pkt_offset=%pkt_offset%, pull_offset=%pull_offset%")

bind_layers(IP, LNIC, proto=LNIC_PROTO)

