
from scapy.all import *

START_RX_TYPE = 0
START_TX_TYPE = 1
DATA_TYPE = 2
DONE_TYPE = 3

class Throughput(Packet):
    name = "Throughput"
    fields_desc = [
        LongField("msg_type", START_RX_TYPE)
    ]

class StartRx(Packet):
    name = "StartRx"
    fields_desc = [
        LongField("num_msgs", 0)
    ]

class StartTx(Packet):
    name = "StartRx"
    fields_desc = [
        LongField("num_msgs", 0),
        LongField("msg_size", 0)
    ]

bind_layers(Throughput, StartRx, msg_type=START_RX_TYPE)
bind_layers(Throughput, StartTx, msg_type=START_TX_TYPE)
