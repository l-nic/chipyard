
from scapy.all import *
import struct

LNIC_PROTO = 0x99

class LNIC(Packet):
    name = "LNIC"
    fields_desc = [
        ShortField("src_context", 0),
        ShortField("dst_context", 0),
        ShortField("msg_len", 0),
        ByteField("pkt_offset", 0),
        ShortField("tx_msg_id", 0),
        ShortField("buf_ptr", 0),
        ByteField("buf_size_class", 0),
        ShortField("padding", 0),
    ]

bind_layers(IP, LNIC, proto=LNIC_PROTO)

