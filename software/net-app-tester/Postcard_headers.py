
from scapy.all import *

UPSTREAM_COLLECTOR_PORT = 0x1111

"""Format of the raw postcard sent from the switches to the
   collector for the software only collector.
"""
class RawPostcardSW(Packet):
    name = "RawPostcardSW"
    fields_desc = [
        # word 1
        IPField("dst_ip", "0.0.0.0"),
        ShortField("dst_port", 0),
        ShortField("tx_msg_id", 0),
        # word 2
        LongField("pkt_offset", 0),
        # word 3
        LongField("qtime", 0),
        # word 4
        LongField("timestamp", 0)
    ]

class AggPostcard(Packet):
    name = "AggPostcard"
    fields_desc = [
        # word 1
        IPField("dst_ip", "0.0.0.0"),
        ShortField("dst_port", 0),
        ShortField("tx_msg_id", 0),
        # word 2
        IPField("src_ip", "0.0.0.0"),
        ShortField("src_port", 0),
        ShortField("pkt_offset", 0),
        # word 3
        LongField("total_qtime", 0)
    ]

class DoneMsg(Packet):
    name = "DoneMsg"
    fields_desc = [
        IntField("timestamp", 0),
        IntField("latency", 0)
    ]

