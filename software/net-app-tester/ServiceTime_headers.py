
from scapy.all import *

CONFIG_TYPE = 0
DATA_TYPE = 1

class ServiceTime(Packet):
    name = "ServiceTime"
    fields_desc = [
        LongField("msg_type", CONFIG_TYPE)
    ]

class Config(Packet):
    name = "Config"
    fields_desc = [
        LongField("num_msgs", 0),
        LongField("timestamp", 0)
    ]

class DataReq(Packet):
    name = "DataReq"
    fields_desc = [
        LongField("service_time", 0)
    ]

bind_layers(ServiceTime, Config, msg_type=CONFIG_TYPE)
bind_layers(ServiceTime, DataReq, msg_type=DATA_TYPE)

