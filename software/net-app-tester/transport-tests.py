
import unittest
from scapy.all import *
from NDP_headers import NDP
from Homa_headers import Homa
from LNIC_utils import *
import Loopback_headers as Loopback
import Throughput_headers as Throughput
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

RTT_PKTS = 2

NUM_UNSCHEDULED_PRIOS = 1
OVERCOMMITMENT_LEVEL = 3

LOG_DIR = 'results'

LOOPBACK_TEST_NUM_MSGS = 20
STREAM_TEST_NUM_MSGS = 30

def write_csv(dname, fname, df):
    log_dir = os.path.join(LOG_DIR, dname)
    if not os.path.exists(log_dir):
      os.makedirs(log_dir)
    with open(os.path.join(log_dir, fname), 'w') as f:
        f.write(df.to_csv(index=False))

def print_pkts(pkts):
    for i in range(len(pkts)):
      print "---- Pkt {} ----".format(i)
      pkts[i].show2()
    #   hexdump(pkts[i])

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

def config_msg(proto, num_msgs):
    msg_len = len(Loopback.Loopback() / Loopback.Config())
    return lnic_pkt(proto, msg_len, 0, 0, src_context=LATENCY_CONTEXT, dst_context=0) \
            / Loopback.Loopback() / Loopback.Config(num_msgs=num_msgs)

def data_req(proto, msg_len):
    assert msg_len >= len(Loopback.Loopback())
    pad_bytes = msg_len - len(Loopback.Loopback())
    return lnic_pkt(proto, msg_len, 0, 0, src_context=LATENCY_CONTEXT, dst_context=0) \
            / Loopback.Loopback(msg_type=Loopback.DATA_TYPE) \
            / ('\x00'*pad_bytes)

def wait_boot_pkt(proto, prn):
    """Wait for a boot pkt from the simulated host.
       prn: a function to process each received packet.
    """
    filt = lambda p: p.haslayer(proto) and p[proto].flags.DATA and p[Ether].src == NIC_MAC
    sniffer = AsyncSniffer(iface=TEST_IFACE, lfilter=filt, prn=prn, count=1)
    sniffer.start()
    sniffer.join()

def do_latency_test(proto, msg_len, prn):
    msg = '\x00' * msg_len
    pkt = packetize(proto, msg, tx_msg_id=0, src_context=LATENCY_CONTEXT)
    assert len(pkt)==1, "Latency test can only run for single packet messages"
    pkt = pkt[0]
#        print "*********** Request Pkts: ***********"
#        print_pkts(pkts)
    # start sniffing for responses
    sniffer = AsyncSniffer(iface=TEST_IFACE, lfilter=lambda x: x.haslayer(proto) and x[proto].flags.DATA and x[Ether].src == NIC_MAC,
                prn=prn, count=1, timeout=100)

    sniffer.start()
    time.sleep(1)
    # send in pkts
    sendp(pkt, iface=TEST_IFACE)
    # wait for all response pkts
    sniffer.join()
    assert len(sniffer.results)==1, "Unexpected number of packets ({})have been received.".format(len(sniffer.results))
#        print "*********** Response Pkts: ***********"
#        print_pkts(sniffer.results)
    return sniffer.results

def latency_test(proto, prn, fname):
    # Wait for boot pkt before running the test
    wait_boot_pkt(proto, prn)
    msg_lens = [16, 32, 64, 128, 256, 512, 1024]
    length = []
    latency = []
    for l in msg_lens:
        for i in range(LOOPBACK_TEST_NUM_MSGS):
            print 'Testing msg_len = {} bytes'.format(l)
            length.append(l)
            rx_pkts = do_latency_test(proto, l, prn)
            p = rx_pkts[0]
            lat = struct.unpack('!L', str(p)[-4:])[0]
            time = struct.unpack('!L', str(p)[-8:-4])[0]
            latency.append(lat)
    # record latencies
    df = pd.DataFrame({'msg_len':length, 'latency':latency})
    write_csv('latency', fname, df)

def latency_test_basic(proto, prn):
    wait_boot_pkt(proto, prn)
    msg_len = 16
    print 'Testing msg_len = {} bytes'.format(msg_len)
    rx_pkts = do_latency_test(proto, msg_len, prn)    
    p = rx_pkts[0]
    lat = struct.unpack('!L', str(p)[-4:])[0]
    print 'Latency = {} cycles = {} ns'.format(lat, lat/3.2)

def do_throughput_test(proto, msg_len, prn):
    inputs = []
    inputs.append(config_msg(proto, LOOPBACK_TEST_NUM_MSGS))
    # generate req msgs
    inputs += [data_req(proto, msg_len) for i in range(LOOPBACK_TEST_NUM_MSGS)]
    # assign unique ID to each msg
    for p, i in zip(inputs, range(len(inputs))):
        p[proto].tx_msg_id = i
    # start sniffing for responses
    sniffer = AsyncSniffer(iface=TEST_IFACE, lfilter=lambda x: x.haslayer(proto) and x[proto].flags.DATA and x[Ether].dst == MY_MAC,
                prn=prn, count=LOOPBACK_TEST_NUM_MSGS, timeout=100)
    sniffer.start()
    time.sleep(1)
    # send inputs get response
    sendp(inputs, iface=TEST_IFACE)
    # wait for all responses
    sniffer.join()
    p = sniffer.results[-1]

    total_latency = struct.unpack('!L', str(p)[-4:])[0]
    throughput = sum([len(p) for p in inputs])/float(total_latency) # bytes/cycle
    throughput_pps = len(inputs)/(float(total_latency) * 0.3125e-3)
    throughput_gbps = throughput*8/0.3125
    print "total latency = {} cycles".format(total_latency)
    print "throughput = {} bytes/cycle = {} Mpps = {} Gbps".format(throughput, throughput_pps, throughput_gbps)
    return throughput_gbps

def throughput_test(proto, prn, fname):
    # Wait for boot pkt before running the test
    wait_boot_pkt(proto, prn)
    msg_len = [16, 32, 64, 128, 256, 512, 1024]
    tput = []
    for l in msg_len:
        tput.append(do_throughput_test(proto, l, prn))
    # record throughputs
    df = pd.DataFrame({'msg_len':msg_len, 'throughput':tput})
    write_csv('throughput', fname, df)

def do_stream_throughput_test(proto, msg_len, prn):
    inputs = []
    inputs.append(config_msg(proto, STREAM_TEST_NUM_MSGS))
    # generate req msgs
    inputs += [data_req(proto, msg_len) for i in range(STREAM_TEST_NUM_MSGS)]
    # assign unique ID to each msg
    for p, i in zip(inputs, range(len(inputs))):
        p[proto].tx_msg_id = i
    # start sniffing for responses
    sniffer = AsyncSniffer(iface=TEST_IFACE, lfilter=lambda x: x.haslayer(proto) and x[proto].flags.DATA and x[Ether].dst == MY_MAC,
                prn=prn, count=STREAM_TEST_NUM_MSGS, timeout=100)
    sniffer.start()
    time.sleep(1)
    # send inputs get response
    sendp(inputs, iface=TEST_IFACE)
    # wait for all responses
    sniffer.join()
    p = sniffer.results[-1]

    total_latency = struct.unpack('!L', str(p)[-4:])[0]
    throughput = sum([len(p) for p in inputs])/float(total_latency) # bytes/cycle
    throughput_pps = len(inputs)/(float(total_latency) * 0.3125e-3)
    throughput_gbps = throughput*8/0.3125
    print "total latency = {} cycles".format(total_latency)
    print "throughput = {} bytes/cycle = {} Mpps = {} Gbps".format(throughput, throughput_pps, throughput_gbps)
    return throughput_gbps

def stream_throughput_test(proto, prn, fname):
    # Wait for boot pkt before running the test
    wait_boot_pkt(proto, prn)
    msg_len = [32, 64, 128, 256, 512, 1024]
    tput = []
    for l in msg_len:
        tput.append(do_stream_throughput_test(proto,l, prn))
    # record throughputs
    df = pd.DataFrame({'msg_len':msg_len, 'throughput':tput})
    write_csv('stream', fname, df)

class NdpTest(unittest.TestCase):

    def test_latency(self):
        receiver = NDPReceiver(TEST_IFACE) # Need to send NDP control packets
        latency_test(proto=NDP, prn=receiver.process_pkt, fname='ndp_msg_len_latency.csv')

    def test_latency_basic(self):
        receiver = NDPReceiver(TEST_IFACE)
        latency_test_basic(proto=NDP, prn=receiver.process_pkt)

    def test_throughput(self):
        receiver = NDPReceiver(TEST_IFACE) # Need to send NDP control packets
        throughput_test(proto=NDP, prn=receiver.process_pkt, fname='ndp_msg_len_throughput.csv')

    def test_stream_throughput(self):
        receiver = NDPReceiver(TEST_IFACE) # Need to send NDP control packets
        stream_throughput_test(proto=NDP, prn=receiver.process_pkt, fname='ndp_msg_len_stream_throughput.csv')

class HomaTest(unittest.TestCase):

    def test_latency(self):
        receiver = HomaReceiver(TEST_IFACE) # Need to send Homa control packets
        latency_test(proto=Homa, prn=receiver.process_pkt, fname='homa_msg_len_latency.csv')

    def test_latency_basic(self):
        receiver = HomaReceiver(TEST_IFACE)
        latency_test_basic(proto=Homa, prn=receiver.process_pkt)

    def test_throughput(self):
        receiver = HomaReceiver(TEST_IFACE) # Need to send Homa control packets
        throughput_test(proto=Homa, prn=receiver.process_pkt, fname='homa_msg_len_throughput.csv')

    def test_stream_throughput(self):
        receiver = HomaReceiver(TEST_IFACE) # Need to send Homa control packets
        stream_throughput_test(proto=Homa, prn=receiver.process_pkt, fname='homa_msg_len_stream_throughput.csv')

    def test_receiver_1_pkt_msg(self):
        print "----------------------Test Receiver - 1 Pkt Msg-----------------------\n"
        # Generate test packets
        msg_len = MAX_SEG_LEN_BYTES # bytes
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

        resp_pkts = sniffer.results

        print "**** Received Packet ****"
        print_pkts(resp_pkts)

        # Verify that the correct msg and pkt offset is ACKed
        ack = resp_pkts[0]
        self.assertEqual(tx_msg_id, ack[Homa].tx_msg_id)
        self.assertEqual(compute_num_pkts(msg_len), ack[Homa].pkt_offset)

    def test_receiver_2_pkt_msg(self):
        print "----------------------Test Receiver - 2 Pkt Msg-----------------------\n"
        # Generate test packets
        msg_len = MAX_SEG_LEN_BYTES*2 # bytes
        msg = '\x00'*msg_len
        tx_msg_id = 0
        pkts = packetize(Homa, msg, tx_msg_id)

        print "**** Transmitted Packets ****"
        print_pkts(pkts)

        expected_num_pkts = 2 

        # start sniffing for response packets
        sniffer = AsyncSniffer(iface=TEST_IFACE, lfilter=lambda x: x.haslayer(Homa) and x[Ether].src == NIC_MAC,
                    count=expected_num_pkts, timeout=100)
        sniffer.start()
        time.sleep(1)
        # send in pkts
        sendp(pkts, iface=TEST_IFACE)
        # wait for all response pkts
        sniffer.join()

        resp_pkts = sniffer.results

        print "**** Received Packet ****"
        print_pkts(resp_pkts)

        # Verify that the correct pkts are received
        self.assertEqual(len(resp_pkts),expected_num_pkts)
        
        next_pkt_offset = 1
        for ack in resp_pkts:
            self.assertEqual(ack[Homa].flags, "ACK")
            self.assertEqual(ack[Homa].tx_msg_id, tx_msg_id)
            self.assertEqual(ack[Homa].pkt_offset, next_pkt_offset)
            next_pkt_offset += 1

    def test_receiver_4_pkt_msg(self):
        print "----------------------Test Receiver - 4 Pkt Msg-----------------------\n"
        # Generate test packets
        num_pkts = 4
        msg_len = MAX_SEG_LEN_BYTES * num_pkts # bytes
        msg = '\x00'*msg_len
        tx_msg_id = 0
        pkts = packetize(Homa, msg, tx_msg_id)

        print "**** Transmitted Packets ****"
        print_pkts(pkts)

        expected_num_pkts = num_pkts 

        # start sniffing for response packets
        sniffer = AsyncSniffer(iface=TEST_IFACE, lfilter=lambda x: x.haslayer(Homa) and x[Ether].src == NIC_MAC,
                    count=expected_num_pkts, timeout=100)
        sniffer.start()
        time.sleep(1)
        # send in pkts
        sendp(pkts, iface=TEST_IFACE)
        # wait for all response pkts
        sniffer.join()

        resp_pkts = sniffer.results

        print "**** Received Packet ****"
        print_pkts(resp_pkts)

        # Verify that the correct pkts are received
        self.assertEqual(len(resp_pkts),expected_num_pkts)
        
        next_pkt_offset = 1
        for p in resp_pkts:
            if next_pkt_offset + RTT_PKTS <= num_pkts:
                self.assertEqual(p[Homa].flags, "GRANT")
                self.assertEqual(p[Homa].grant_offset, next_pkt_offset + RTT_PKTS)
                self.assertEqual(p[Homa].grant_prio, NUM_UNSCHEDULED_PRIOS)
            else:
                self.assertEqual(p[Homa].flags, "ACK")
            self.assertEqual(p[Homa].tx_msg_id, tx_msg_id)
            self.assertEqual(p[Homa].pkt_offset, next_pkt_offset)
            
            next_pkt_offset += 1

    def test_receiver_4_pkt_msg_nack(self):
        print "-------------------Test Receiver - 4 Pkt Msg NACK---------------------\n"
        # Generate test packets
        num_pkts = 4
        msg_len = MAX_SEG_LEN_BYTES * num_pkts # bytes
        msg = '\x00'*msg_len
        tx_msg_id = 0
        pkts = packetize(Homa, msg, tx_msg_id)

        next_pkt_offset = 0
        print ("**** Transmitting pkt {} of the msg".format(next_pkt_offset))
        # print_pkts([pkts[next_pkt_offset]])

        expected_num_pkts = 1 
        sniffer = AsyncSniffer(iface=TEST_IFACE, lfilter=lambda x: x.haslayer(Homa) and x[Ether].src == NIC_MAC,
                    count=expected_num_pkts, timeout=100)
        sniffer.start()
        time.sleep(1)
        sendp(pkts[next_pkt_offset], iface=TEST_IFACE)
        sniffer.join()
        resp_pkts = sniffer.results

        print "**** Received Packet ****"
        print_pkts(resp_pkts)
        # Verify that the correct pkts are received
        self.assertEqual(len(resp_pkts),expected_num_pkts)
        
        next_pkt_offset += 1
        self.assertEqual(resp_pkts[0][Homa].flags, "GRANT")
        self.assertEqual(resp_pkts[0][Homa].grant_offset, next_pkt_offset + RTT_PKTS)
        self.assertEqual(resp_pkts[0][Homa].grant_prio, NUM_UNSCHEDULED_PRIOS)
        self.assertEqual(resp_pkts[0][Homa].tx_msg_id, tx_msg_id)
        self.assertEqual(resp_pkts[0][Homa].pkt_offset, next_pkt_offset)

        print ("**** Transmitting pkt {} of the msg as chopped".format(next_pkt_offset))
        chop_pkt = pkts[next_pkt_offset].copy()
        chop_pkt[Homa].flags = "DATA+CHOP"
        chop_pkt[Homa].remove_payload()
        chop_pkt = chop_pkt / Raw('\x00')
        # print_pkts([chop_pkt])

        expected_num_pkts = 1 
        sniffer = AsyncSniffer(iface=TEST_IFACE, lfilter=lambda x: x.haslayer(Homa) and x[Ether].src == NIC_MAC,
                    count=expected_num_pkts, timeout=100)
        sniffer.start()
        time.sleep(1)
        sendp(chop_pkt, iface=TEST_IFACE)
        sniffer.join()
        resp_pkts = sniffer.results

        print "**** Received Packet ****"
        print_pkts(resp_pkts)
        # Verify that the correct pkts are received
        self.assertEqual(len(resp_pkts),expected_num_pkts)

        self.assertEqual(resp_pkts[0][Homa].flags, "NACK")
        # self.assertEqual(resp_pkts[0][Homa].grant_offset, next_pkt_offset + RTT_PKTS)
        # self.assertEqual(resp_pkts[0][Homa].grant_prio, NUM_UNSCHEDULED_PRIOS)
        self.assertEqual(resp_pkts[0][Homa].tx_msg_id, tx_msg_id)
        self.assertEqual(resp_pkts[0][Homa].pkt_offset, next_pkt_offset)

        print ("**** Transmitting pkt {} of the msg".format(next_pkt_offset))
        # print_pkts([pkts[next_pkt_offset]])

        expected_num_pkts = 1 
        sniffer = AsyncSniffer(iface=TEST_IFACE, lfilter=lambda x: x.haslayer(Homa) and x[Ether].src == NIC_MAC,
                    count=expected_num_pkts, timeout=100)
        sniffer.start()
        time.sleep(1)
        sendp(pkts[next_pkt_offset], iface=TEST_IFACE)
        sniffer.join()
        resp_pkts = sniffer.results

        print "**** Received Packet ****"
        print_pkts(resp_pkts)
        # Verify that the correct pkts are received
        self.assertEqual(len(resp_pkts),expected_num_pkts)
        
        next_pkt_offset += 1
        self.assertEqual(resp_pkts[0][Homa].flags, "GRANT")
        self.assertEqual(resp_pkts[0][Homa].grant_offset, next_pkt_offset + RTT_PKTS)
        self.assertEqual(resp_pkts[0][Homa].grant_prio, NUM_UNSCHEDULED_PRIOS)
        self.assertEqual(resp_pkts[0][Homa].tx_msg_id, tx_msg_id)
        self.assertEqual(resp_pkts[0][Homa].pkt_offset, next_pkt_offset)

        # Send the rest of the packets to make sure no state stays idle in the receiver
        sendp(pkts[next_pkt_offset:], iface=TEST_IFACE)

    def test_receiver_2_msgs(self):
        print "---------------------Test Receiver - 2 Msgs---------------------\n"

        # Generate test messages
        s_msg_num_pkts = 4
        s_msg_len = MAX_SEG_LEN_BYTES * s_msg_num_pkts # bytes
        s_msg = '\x00' * s_msg_len
        s_tx_msg_id = 0
        s_pkts = packetize(Homa, s_msg, s_tx_msg_id)

        l_msg_num_pkts = 6
        l_msg_len = MAX_SEG_LEN_BYTES * l_msg_num_pkts # bytes
        l_msg = '\x00' * l_msg_len
        l_tx_msg_id = s_tx_msg_id + 1
        l_pkts = packetize(Homa, l_msg, l_tx_msg_id)

        # send in the first pkt of the long msg and expect to receive a GRANT
        pkt_offset_to_send = 0
        # print "**** Transmitted Packets ****"
        # print_pkts([l_pkts[pkt_offset_to_send]])
        print ("**** Transmitting pkt {} of the long msg".format(pkt_offset_to_send))
        expected_num_pkts = 1
        sniffer = AsyncSniffer(iface=TEST_IFACE, lfilter=lambda x: x.haslayer(Homa) and x[Ether].src == NIC_MAC,
                    count=expected_num_pkts, timeout=100)
        sniffer.start()
        time.sleep(1)
        sendp(l_pkts[pkt_offset_to_send], iface=TEST_IFACE)
        sniffer.join()
        resp_pkt = sniffer.results
        print "**** Received Packet ****"
        print_pkts(resp_pkt)
        self.assertEqual(len(resp_pkt),expected_num_pkts)
        self.assertEqual(resp_pkt[0][Homa].flags, "GRANT")
        self.assertEqual(resp_pkt[0][Homa].pkt_offset, pkt_offset_to_send + 1)
        self.assertEqual(resp_pkt[0][Homa].grant_offset, pkt_offset_to_send + RTT_PKTS + 1)
        self.assertEqual(resp_pkt[0][Homa].grant_prio, NUM_UNSCHEDULED_PRIOS)
        self.assertEqual(resp_pkt[0][Homa].tx_msg_id, l_tx_msg_id)

        # send in the first pkt of the short msg and expect to receive a GRANT
        # print "**** Transmitted Packets ****"
        # print_pkts([s_pkts[pkt_offset_to_send]])
        print ("**** Transmitting pkt {} of the short msg".format(pkt_offset_to_send))
        expected_num_pkts = 1
        sniffer = AsyncSniffer(iface=TEST_IFACE, lfilter=lambda x: x.haslayer(Homa) and x[Ether].src == NIC_MAC,
                    count=expected_num_pkts, timeout=100)
        sniffer.start()
        time.sleep(1)
        sendp(s_pkts[pkt_offset_to_send], iface=TEST_IFACE)
        sniffer.join()
        resp_pkt = sniffer.results
        print "**** Received Packet ****"
        print_pkts(resp_pkt)
        self.assertEqual(len(resp_pkt),expected_num_pkts)
        self.assertEqual(resp_pkt[0][Homa].flags, "GRANT")
        self.assertEqual(resp_pkt[0][Homa].pkt_offset, pkt_offset_to_send + 1)
        self.assertEqual(resp_pkt[0][Homa].grant_offset, pkt_offset_to_send + RTT_PKTS + 1)
        self.assertEqual(resp_pkt[0][Homa].grant_prio, NUM_UNSCHEDULED_PRIOS)
        self.assertEqual(resp_pkt[0][Homa].tx_msg_id, s_tx_msg_id)

        # send in the second pkt of the long msg and expect to receive a GRANT
        pkt_offset_to_send = 1
        # print "**** Transmitted Packets ****"
        # print_pkts([l_pkts[pkt_offset_to_send]])
        print ("**** Transmitting pkt {} of the long msg".format(pkt_offset_to_send))
        expected_num_pkts = 1
        sniffer = AsyncSniffer(iface=TEST_IFACE, lfilter=lambda x: x.haslayer(Homa) and x[Ether].src == NIC_MAC,
                    count=expected_num_pkts, timeout=100)
        sniffer.start()
        time.sleep(1)
        sendp(l_pkts[pkt_offset_to_send], iface=TEST_IFACE)
        sniffer.join()
        resp_pkt = sniffer.results
        print "**** Received Packet ****"
        print_pkts(resp_pkt)
        self.assertEqual(len(resp_pkt),expected_num_pkts)
        self.assertEqual(resp_pkt[0][Homa].flags, "GRANT")
        self.assertEqual(resp_pkt[0][Homa].pkt_offset, pkt_offset_to_send + 1)
        self.assertEqual(resp_pkt[0][Homa].grant_offset, pkt_offset_to_send + RTT_PKTS + 1)
        self.assertEqual(resp_pkt[0][Homa].grant_prio, NUM_UNSCHEDULED_PRIOS + 1)
        self.assertEqual(resp_pkt[0][Homa].tx_msg_id, l_tx_msg_id)



        # Send the rest of the packets to make sure no state stays idle in the receiver
        # print "**** Transmitted Packets ****"
        # print_pkts(l_pkts[2:])
        print ("**** Transmitting pkts {} to {} of the long msg".format(2,l_msg_num_pkts))
        expected_num_pkts = len(l_pkts[2:])
        sniffer = AsyncSniffer(iface=TEST_IFACE, lfilter=lambda x: x.haslayer(Homa) and x[Ether].src == NIC_MAC,
                    count=expected_num_pkts, timeout=100)
        sniffer.start()
        time.sleep(1)
        sendp(l_pkts[2:], iface=TEST_IFACE)
        sniffer.join()
        resp_pkt = sniffer.results
        # print "**** Received Packet ****"
        # print_pkts(resp_pkt)
        self.assertEqual(len(resp_pkt),expected_num_pkts)

        # Send the rest of the packets to make sure no state stays idle in the receiver
        # print "**** Transmitted Packets ****"
        # print_pkts(s_pkts[1:])
        print ("**** Transmitting pkts {} to {} of the short msg".format(1,s_msg_num_pkts))
        expected_num_pkts = len(s_pkts[1:])
        sniffer = AsyncSniffer(iface=TEST_IFACE, lfilter=lambda x: x.haslayer(Homa) and x[Ether].src == NIC_MAC,
                    count=expected_num_pkts, timeout=100)
        sniffer.start()
        time.sleep(1)
        sendp(s_pkts[1:], iface=TEST_IFACE)
        sniffer.join()
        resp_pkt = sniffer.results
        # print "**** Received Packet ****"
        # print_pkts(resp_pkt)
        self.assertEqual(len(resp_pkt),expected_num_pkts)

    def test_receiver_10_msgs_completion(self):
        print "--------------Test Receiver - 10 Msgs Completion----------------\n"

        msg_sizes_pkts = [32,30,28,26,24,22,20,18,16,14]
        msgs = []
        # Generate msgs
        tx_msg_id = 0
        for msg_size_pkts in msg_sizes_pkts:
            msg_len = MAX_SEG_LEN_BYTES * msg_size_pkts # bytes
            msg = '\x00'*msg_len
            msgs.append(packetize(Homa, msg, tx_msg_id))
            tx_msg_id += 1

        for msg_idx in range(len(msgs)):
            expected_num_pkts = len(msgs[msg_idx])-1 
            print ("**** Transmitting the first {} pkts of msg {}".format(expected_num_pkts,msg_idx))
            sniffer = AsyncSniffer(iface=TEST_IFACE, lfilter=lambda x: x.haslayer(Homa) and x[Ether].src == NIC_MAC,
                    count=expected_num_pkts, timeout=100)
            sniffer.start()
            time.sleep(1)
            sendp(msgs[msg_idx][:expected_num_pkts], iface=TEST_IFACE)
            sniffer.join()
            resp_pkts = sniffer.results
            print ("** Received {} pkts".format(len(resp_pkts)))
            self.assertEqual(len(resp_pkts),expected_num_pkts)
        
        for msg_idx in range(len(msgs)):
            expected_num_pkts = 1 
            print ("**** Transmitting the last pkt of msg {}".format(msg_idx))
            sniffer = AsyncSniffer(iface=TEST_IFACE, lfilter=lambda x: x.haslayer(Homa) and x[Ether].src == NIC_MAC,
                    count=expected_num_pkts, timeout=100)
            sniffer.start()
            time.sleep(1)
            sendp(msgs[msg_idx][-1], iface=TEST_IFACE)
            sniffer.join()
            resp_pkts = sniffer.results
            print ("** Received {} pkts".format(len(resp_pkts)))
            self.assertEqual(len(resp_pkts),expected_num_pkts)
            self.assertEqual(resp_pkts[0][Homa].flags, "ACK")
            self.assertEqual(resp_pkts[0][Homa].tx_msg_id, msg_idx)
            self.assertEqual(resp_pkts[0][Homa].pkt_offset, len(msgs[msg_idx]))
