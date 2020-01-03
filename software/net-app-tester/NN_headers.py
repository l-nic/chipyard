
from scapy.all import *

CONFIG_TYPE = 0
WEIGHT_TYPE = 1
DATA_TYPE = 2

class NN(Packet):
    name = "NN"
    fields_desc = [
        LongField("type", CONFIG_TYPE)
    ]

class Config(Packet):
    name = "Config"
    fields_desc = [
        LongField("num_edges", 0)
    ]

class Weight(Packet):
    name = "Weight"
    fields_desc = [
        LongField("index", 0),
        LongField("weight", 0)
    ]

class Data(Packet):
    name = "Data"
    fields_desc = [
        LongField("index", 0),
        LongField("data", 0)
    ]

bind_layers(NN, Config, type=CONFIG_TYPE)
bind_layers(NN, Weight, type=WEIGHT_TYPE)
bind_layers(NN, Data,   type=DATA_TYPE)

