
from scapy.all import *
from NDP_headers import NDP
from Homa_headers import Homa

IFACE = "tap0"

sniffer = AsyncSniffer(iface=IFACE, prn=lambda p: p.show())
sniffer.start()
sniffer.join()

