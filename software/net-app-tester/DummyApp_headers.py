
from scapy.all import *

class DummyApp(Packet):
    name = "DummyApp"
    fields_desc = [
        LongField("service_time", 0)
    ]

