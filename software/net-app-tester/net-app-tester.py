#!/usr/bin/env python

import unittest
from scapy.all import *
from LNIC_headers import LNIC

TEST_IFACE = "tap0"
TIMEOUT_SEC = 5 # seconds

NIC_MAC = "08:11:22:33:44:08"
MY_MAC = "08:55:66:77:88:08"

NIC_IP = "10.0.0.1"
MY_IP = "10.1.2.3"

DST_CONTEXT = 0
MY_CONTEXT = 1

# Test to check basic loopback functionality
class SimpleLoopback(unittest.TestCase):

    def test_loopback(self):
        msg_len = 64 # bytes
        req = Ether(dst=NIC_MAC, src=MY_MAC) / \
                IP(src=MY_IP, dst=NIC_IP) / \
                LNIC(src=MY_CONTEXT, dst=DST_CONTEXT, msg_len=msg_len) / ('\x00'*msg_len)
        print "---------- Request -------------"
        req.show2()
        # send request / receive response
        resp = srp1(req, iface=TEST_IFACE, timeout=TIMEOUT_SEC)
        self.assertTrue(resp is not None)
        print "---------- Response -------------"
        resp.show2()


if __name__ == '__main__':
    unittest.main()
