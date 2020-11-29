
from scapy.all import *

CONFIG_TYPE = 0
DATA_TYPE = 1
RESP_TYPE = 2

class DotProd(Packet):
    name = "DotProd"
    fields_desc = [
        LongField("msg_type", CONFIG_TYPE)
    ]

class Config(Packet):
    name = "Config"
    fields_desc = [
        LongField("num_msgs", 0),
        LongField("timestamp", 0)
    ]

class Data(Packet):
    name = "Data"
    fields_desc = [
        LongField("num_words", 0),
        FieldListField("words", [], LongField("", 0), count_from=lambda p:p.num_words),
        LongField("timestamp", 0)
    ]

class Resp(Packet):
    name = "Resp"
    fields_desc = [
        LongField("result", 0),
        IntField("timestamp", 0),
        IntField("latency", 0)
    ]

bind_layers(DotProd, Config, msg_type=CONFIG_TYPE)
bind_layers(DotProd, Data, msg_type=DATA_TYPE)
bind_layers(DotProd, Resp, msg_type=RESP_TYPE)

