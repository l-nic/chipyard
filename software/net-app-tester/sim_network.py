
from scapy.all import *
from LNIC_headers import LNIC
from heapq import heappush, heappop
import time

IFACE = "tap0"

# NOTE: this must agree with the egress P4 program
SWITCH_MAC = "08:55:66:77:88:08"
NIC_MAC = "08:11:22:33:44:08"

class NetworkPkt(object):
    """A small wrapper class around scapy pkts to add departure_time"""
    def __init__(self, pkt, departure_time):
        self.pkt = pkt
        self.departure_time = departure_time

    def __lt__(self, other):
        """Highest priority element is the one with the smallest departure_time"""
        return self.departure_time < other.departure_time

class SimNetwork:
    """Simulate a network connected to a Linux TAP interface.
       Receive pkts, delay them, trim/drop them, then send them back.
    """
    def __init__(self):
        # heap of pkts sorted by departure time
        self.scheduled_pkts = []

        # start receiving pkts
        filt = lambda x: x[Ether].dst == SWITCH_MAC # only sniff inbound pkts
        self.sniffer = AsyncSniffer(iface=IFACE, lfilter=filt, prn=self.schedule)
        self.sniffer.start()

        # start transmitting scheduled pkts
        self.transmit()

    def schedule(self, pkt):
        """Pick a departure time for the pkt and schedule it.
        """
        print "Scheduling pkt ..."
        # swap MAC addresses
        tmp = pkt[Ether].src
        pkt[Ether].src = pkt[Ether].dst
        pkt[Ether].dst = tmp

        # schedule pkt
        now = time.time()
        departure_time = now + 1.0
        heappush(self.scheduled_pkts, NetworkPkt(pkt, departure_time))

    def transmit(self):
        """Monitor the head of the scheduled pkts heap to see if it's time to send it.
        """
        try:
            while True:
                if (len(self.scheduled_pkts) > 0 and time.time() >= self.scheduled_pkts[0].departure_time):
                    print "Sending pkt ..."
                    net_pkt = heappop(self.scheduled_pkts)
                    sendp(net_pkt.pkt, iface=IFACE)
                else:
                    time.sleep(1.0)
        except KeyboardInterrupt as e:
            self.sniffer.stop()
            print 'All received pkts:'
            for p in self.sniffer.results:
                assert LNIC in p, "LNIC header not in sniffed packet!"
                print '{} / DATA ({} bytes)'.format(p.summary(), len(p[LNIC].payload))

def main():
    SimNetwork()

if __name__ == "__main__":
    main()

