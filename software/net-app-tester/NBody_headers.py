
from scapy.all import *

CONFIG_TYPE = 0
TRAVERSAL_REQ_TYPE = 1
TRAVERSAL_RESP_TYPE = 2

class NBody(Packet):
    name = "NBody"
    fields_desc = [
        LongField("msg_type", CONFIG_TYPE)
    ]

class Config(Packet):
    name = "Config"
    fields_desc = [
        IEEEDoubleField("xcom", 0),
        IEEEDoubleField("ycom", 0),
        LongField("num_msgs", 0),
        LongField("timestamp", 0)
    ]

class TraversalReq(Packet):
    name = "TraversalReq"
    fields_desc = [
        IEEEDoubleField("xpos", 0),
        IEEEDoubleField("ypos", 0),
        LongField("timestamp", 0)
    ]

class TraversalResp(Packet):
    name = "TraversalResp"
    fields_desc = [
        IEEEDoubleField("force", 0),
        LongField("timestamp", 0)
    ]

bind_layers(NBody, Config, msg_type=CONFIG_TYPE)
bind_layers(NBody, TraversalReq, msg_type=TRAVERSAL_REQ_TYPE)
bind_layers(NBody, TraversalResp,   msg_type=TRAVERSAL_RESP_TYPE)

