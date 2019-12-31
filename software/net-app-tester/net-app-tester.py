#!/usr/bin/env python

import unittest
from scapy.all import *
from LNIC_headers import LNIC

TEST_IFACE = "tap0"
TIMEOUT_SEC = 2 # seconds

NIC_MAC = "08:11:22:33:44:08"
MY_MAC = "08:55:66:77:88:08"

NIC_IP = "10.0.0.1"
MY_IP = "10.1.2.3"

DST_CONTEXT = 0
MY_CONTEXT = 1

def lnic_req(msg_len):
    return Ether(dst=NIC_MAC, src=MY_MAC) / \
            IP(src=MY_IP, dst=NIC_IP) / \
            LNIC(src=MY_CONTEXT, dst=DST_CONTEXT, msg_len=msg_len)

# Test to check basic loopback functionality
class SimpleLoopback(unittest.TestCase):
    def test_loopback(self):
        msg_len = 64 # bytes
        payload = Raw('\x00'*msg_len)
        req = lnic_req(msg_len) / payload
        print "---------- Request -------------"
        req.show2()
        # send request / receive response
        resp = srp1(req, iface=TEST_IFACE, timeout=TIMEOUT_SEC)
        self.assertIsNotNone(resp)
        print "---------- Response -------------"
        resp.show2()
        self.assertEqual(resp[LNIC].payload, payload)

class NNInference(unittest.TestCase):
    MSG_LEN = 16 # bytes

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

    @staticmethod
    def weight_msg(index, weight):
        return lnic_req(NNInference.MSG_LEN) / NNInference.Weight(index=index, weight=weight)

    @staticmethod
    def data_msg(index, data):
        return lnic_req(NNInference.MSG_LEN) / NNInference.Data(index=index, data=data)

    def test_basic(self):
        # send in weights
        sendp(NNInference.weight_msg(0, 1), iface=TEST_IFACE)
        sendp(NNInference.weight_msg(1, 1), iface=TEST_IFACE)
        sendp(NNInference.weight_msg(2, 1), iface=TEST_IFACE)
        # send in data / receive response
        sendp(NNInference.data_msg(0, 1), iface=TEST_IFACE)
        sendp(NNInference.data_msg(1, 1), iface=TEST_IFACE)
        resp = srp1(NNInference.data_msg(2, 1), iface=TEST_IFACE, timeout=TIMEOUT_SEC)
        # check response
        self.assertIsNotNone(resp)
        print '------------- Response -------------'
        resp.show2()
        self.assertEqual(resp[LNIC].payload, Raw(str(NNInference.Data(index=0, data=3))))


