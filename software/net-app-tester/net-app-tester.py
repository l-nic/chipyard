#!/usr/bin/env python

import unittest
import subprocess
import shlex
import os
import time
import signal
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

#    def setUp(self):
#        config = 'SimNetworkLNICGPRConfig'
#        binary = '/home/vagrant/chipyard/tests/simple-lnic-cpu-loopback-gpr.riscv'
#        sim_binary = '/home/vagrant/chipyard/sims/verilator/simulator-example-{CONFIG}-debug'.format(**{'CONFIG': config})
#        cmd = '{SIM_BINARY} -v /vagrant/{CONFIG}.vcd {BINARY} > sim.log'.format(**{'SIM_BINARY': sim_binary, 'CONFIG': config, 'BINARY': binary})
#        assert os.path.exists(sim_binary), 'Could not find simulation binary'
#        assert os.path.exists(binary), 'Could not find riscv binary'
#        # start the simulation
#        print 'Running cmd: $ {}'.format(cmd)
#        self.sim = subprocess.Popen(shlex.split(cmd))
#        # wait for core to boot
#        time.sleep(3)
##        pkts = sniff(iface=TEST_IFACE, lfilter=lambda x: x.haslayer(LNIC), stop_filter=lambda x: x.haslayer(LNIC), timeout=TIMEOUT_SEC)
##        assert len(pkts) == 1, 'Never received boot msg from core'
#
#    def tearDown(self):
#        #self.sim.send_signal(signal.SIGINT)
#        self.sim.kill()
#        print 'Tear Down Complete.'

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
