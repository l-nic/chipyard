
from scapy.all import *

MAP_TYPE = 0
REDUCE_TYPE = 1

MAP_LEN = 8*6 # bytes
REDUCE_LEN = 8*4 # bytes

class Othello(Packet):
    name = "Othello"
    fields_desc = [
        LongField("type", MAP_TYPE)
    ]

class Map(Packet):
    name = "Map"
    fields_desc = [
        LongField("board", 0),
        LongField("max_depth", 0),
        LongField("cur_depth", 0),
        LongField("src_host_id", 0),
        LongField("src_msg_ptr", 0)
    ]

class Reduce(Packet):
    name = "Reduce"
    fields_desc = [
        LongField("target_host_id", 0),
        LongField("target_msg_ptr", 0),
        LongField("minimax_val", 0)
    ]

bind_layers(Othello, Map, type=MAP_TYPE)
bind_layers(Othello, Reduce, type=REDUCE_TYPE)

