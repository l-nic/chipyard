
import unittest
from scapy.all import *
from NDP_headers import NDP
from Homa_headers import Homa
from LNIC_utils import *
import struct
import pandas as pd
import os
import random
import numpy as np
import time

# set random seed for consistent sims
random.seed(1)
np.random.seed(1)

TEST_IFACE = "tap0"

NIC_MAC = "08:11:22:33:44:08"
MY_MAC = "08:55:66:77:88:08"

NIC_IP = "10.0.0.2"
MY_IP = "10.0.0.1"

DST_CONTEXT = 0
LATENCY_CONTEXT = 0x1234 # use this when we want the HW to insert timestamps into DATA pkts
DEFAULT_CONTEXT = 0x5678

def lnic_pkt(proto, msg_len, pkt_offset, tx_msg_id, src_context=DEFAULT_CONTEXT, dst_context=DST_CONTEXT, src_ip=MY_IP):
    """proto should be either NDP or Homa.
    """
    return Ether(dst=NIC_MAC, src=MY_MAC) / \
            IP(src=src_ip, dst=NIC_IP) / \
            proto(flags='DATA', src_context=src_context, dst_context=dst_context, msg_len=msg_len, pkt_offset=pkt_offset, tx_msg_id=tx_msg_id)

def packetize(proto, msg, tx_msg_id, src_context=DEFAULT_CONTEXT, dst_context=DST_CONTEXT, src_ip=MY_IP):
    """Generate NDP or Homa pkts for the given msg.
       proto should be either NDP or Homa.
    """
    num_pkts = compute_num_pkts(len(msg))
    pkts = []
    for i in range(num_pkts-1):
        p = lnic_pkt(proto, len(msg), i, tx_msg_id, src_context, dst_context, src_ip) / Raw(msg[i*MAX_SEG_LEN_BYTES:(i+1)*MAX_SEG_LEN_BYTES])
        pkts.append(p)
    p = lnic_pkt(proto, len(msg), num_pkts-1, tx_msg_id, src_context, dst_context, src_ip) / Raw(msg[(num_pkts-1)*MAX_SEG_LEN_BYTES:])
    pkts.append(p)
    return pkts

def print_pkts(pkts):
    for i in range(len(pkts)):
      print "---- Pkt {} ----".format(i)
      pkts[i].show2()
      hexdump(pkts[i])


class HomaTest(unittest.TestCase):

    def test_receiver_basic(self):
        # Generate test packets
        msg_len = 1024 # bytes
        msg = '\x00'*msg_len
        tx_msg_id = 0
        pkts = packetize(Homa, msg, tx_msg_id)

        print "**** Transmitted Packets ****"
        print_pkts(pkts)

        # We expect to receive one ACK and no GRANTs for this simple single pkt msg.
        # NOTE: the test application should not send us any DATA pkts so there's no
        # need for us to send ACKs and GRANTs back.
        expected_num_pkts = 1 

        # start sniffing for response packets
        sniffer = AsyncSniffer(iface=TEST_IFACE, lfilter=lambda x: x.haslayer(Homa) and x[Ether].src == NIC_MAC,
                    count=expected_num_pkts, timeout=100)
        sniffer.start()
        time.sleep(1)
        # send in pkts
        sendp(pkts, iface=TEST_IFACE)
        # wait for all response pkts
        sniffer.join()

        # Verify that the correct msg and pkt offset is ACKed
        ack = sniffer.results[0]
        self.assertEqual(tx_msg_id, ack[Homa].tx_msg_id)
        self.assertEqual(0, ack[Homa].pkt_offset)

        print "**** Received Packet ****"
        ack.show()

