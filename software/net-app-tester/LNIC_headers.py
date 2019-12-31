
from scapy.all import *

LNIC_PROTO = 0x99

class LNIC(Packet):
    name = "LNIC"
    fields_desc = [
        ShortField("src", 0),
        ShortField("dst", 0),
        ShortField("msg_id", 0),
        ShortField("msg_len", 0),
        ShortField("offset", 0),
        IntField("padding", 0)
    ]
    def mysummary(self):
        return self.sprintf("src=%src% dst=%dst% msg_id=%msg_id% msg_len=%msg_len% offset=%offset%")
    

bind_layers(IP, LNIC, proto=LNIC_PROTO)

