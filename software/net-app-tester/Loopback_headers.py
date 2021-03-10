
from scapy.all import *

CONFIG_TYPE = 0
DATA_TYPE = 1

class Loopback(Packet):
    name = "Loopback"
    fields_desc = [
        LongField("msg_type", CONFIG_TYPE)
    ]

class Config(Packet):
    name = "Config"
    fields_desc = [
        LongField("num_msgs", 0),
        LongField("timestamp", 0)
    ]

bind_layers(Loopback, Config, msg_type=CONFIG_TYPE)

