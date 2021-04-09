
from scapy.all import *
from NDP_headers import NDP
from Homa_headers import Homa
from heapq import heappush, heappop
import time

IFACE = "tap0"

# NOTE: this must agree with the egress P4 program
SWITCH_MAC = "08:55:66:77:88:08"
NIC_MAC = "08:11:22:33:44:08"

TRIM_FREQ = 4 # DATA pkts
DATA_DROP_FREQ = 0 # DATA pkts
CTRL_DROP_FREQ = 0 # Control pkts (not including trimmed pkts, just ACK, NACK, and PULL pkts)

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

        self.data_pkt_counter = 0
        self.ctrl_pkt_counter = 0
        self.pkt_log = []

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

        drop = False

        proto = None
        if pkt.haslayer(NDP):
          proto = NDP
        elif pkt.haslayer(Homa):
          proto = Homa

        if pkt[proto].flags.DATA:
            self.data_pkt_counter += 1

            # trim data pkts with deterministic frequency
            if TRIM_FREQ > 0 and self.data_pkt_counter % TRIM_FREQ == 0:
                pkt[proto].flags.CHOP = True
                if len(pkt) > 65:
                    pkt = Ether(str(pkt)[0:65])

            if DATA_DROP_FREQ > 0 and self.data_pkt_counter % DATA_DROP_FREQ == 0:
                drop = True

        if not pkt[proto].flags.DATA:
            self.ctrl_pkt_counter += 1

            if CTRL_DROP_FREQ > 0 and self.ctrl_pkt_counter % CTRL_DROP_FREQ == 0:
                drop = True

        if not drop:
            self.pkt_log.append(pkt)
    
            # schedule pkt
            now = time.time()
            departure_time = now + 1.0
            heappush(self.scheduled_pkts, NetworkPkt(pkt, departure_time))
        else:
            print "Dropping pkt: {}".format(pkt.summary())

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
            print 'All TX pkts:'
            for p in self.pkt_log:
                print '{} -- ({} bytes)'.format(p.summary(), len(p))

def main():
    SimNetwork()

if __name__ == "__main__":
    main()

