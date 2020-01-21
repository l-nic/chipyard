
import unittest
from scapy.all import *
from LNIC_headers import LNIC
import NN_headers as NN
import Othello_headers as Othello
import struct
import pandas as pd
import os
import random

TEST_IFACE = "tap0"
TIMEOUT_SEC = 7 # seconds

NIC_MAC = "08:11:22:33:44:08"
MY_MAC = "08:55:66:77:88:08"

NIC_IP = "10.0.0.1"
MY_IP = "10.1.2.3"

DST_CONTEXT = 0
MY_CONTEXT = 0x1234

# Priorities
LOW = 1
HIGH = 0

LOG_DIR = '/vagrant/logs'

def lnic_req(my_context=MY_CONTEXT, lnic_dst = DST_CONTEXT):
    return Ether(dst=NIC_MAC, src=MY_MAC) / \
            IP(src=MY_IP, dst=NIC_IP) / \
            LNIC(src=my_context, dst=lnic_dst)

def write_csv(dname, fname, df):
    log_dir = os.path.join(LOG_DIR, dname)
    if not os.path.exists(log_dir):
      os.makedirs(log_dir)
    with open(os.path.join(log_dir, fname), 'w') as f:
        f.write(df.to_csv(index=False))

class ParallelLoopback(unittest.TestCase):
    def test_range(self):
        # packet_lengths = range(64, 64*20, 64)
        packet_lengths = [64] * 32
        contexts = [(0x1235, 0), (0x1236, 1)]
        print str(len(contexts)*len(packet_lengths))
        sniffer = AsyncSniffer(iface=TEST_IFACE, timeout=20)# count=len(contexts)*len(packet_lengths))
        sniffer.start()
        for pkt_len in packet_lengths:
            for my_context, dst_context in contexts:
                msg_len = pkt_len - len(lnic_req()) # bytes
                payload = Raw('\x00'*msg_len)
                req = lnic_req(my_context=my_context, lnic_dst=dst_context) / payload
                sendp(req, iface=TEST_IFACE)
                time.sleep(.1)
        sniffer.join()
        print sniffer.results
        real_packets = 0
        for resp in sniffer.results:
            self.assertIsNotNone(resp)
            if resp[LNIC].dst == 0x1235 or resp[LNIC].dst == 0x1236:
                resp_data = resp[LNIC].payload
                resp_value = struct.unpack('!Q', str(resp_data)[:8])[0]
                print "Packet from context id " + str(resp[LNIC].src) + " contains value " + str(resp_value)
                real_packets += 1
        print "Total packets received: " + str(real_packets)

class PriorityMix(unittest.TestCase):
    def do_loopback(self, pkt_len, priority):
        msg_len = pkt_len - len(lnic_req()) # bytes
        payload = Raw('\x00'*msg_len)
        req = lnic_req(lnic_dst=priority) / payload
        sendp(req, iface=TEST_IFACE)
    def test_single(self):
        pkt_len = 64
        sniffer = AsyncSniffer(iface=TEST_IFACE, timeout=5)
        sniffer.start()
        self.do_loopback(pkt_len, LOW)
        self.do_loopback(pkt_len, HIGH)
        sniffer.join()
        app_packets = 0
        for resp in sniffer.results:
            self.assertIsNotNone(resp)
            try:
                _ = resp[LNIC].dst
            except IndexError:
                continue
            if resp[LNIC].dst == MY_CONTEXT:
                resp_data = resp[LNIC].payload
                latency = struct.unpack('!Q', str(resp_data)[-8:])[0]
                self.assertTrue(resp[LNIC].src == LOW or resp[LNIC].src == HIGH)
                if resp[LNIC].src == LOW:
                    print 'Low Priority Latency = {} cycles'.format(latency)
                else:
                    print 'High Priority Latency = {} cycles'.format(latency)
                app_packets += 1
        self.assertEqual(app_packets, len([LOW, HIGH]))
    def test_range(self):
        high_priority_target_fraction = 0.8
        num_copies = 2
        latency_low = []
        latency_high = []
        packet_lengths = list(range(64, 64*20, 64))
        packet_lengths = [elem for elem in packet_lengths for i in range(num_copies)]
        priorities = [LOW] * len(packet_lengths)
        for i in range(int(high_priority_target_fraction*len(packet_lengths))):
            priorities[i] = HIGH
        random.shuffle(packet_lengths)
        random.shuffle(priorities)
        print packet_lengths
        print priorities
        sniffer = AsyncSniffer(iface=TEST_IFACE, timeout=30)
        sniffer.start()
        for i in range(len(packet_lengths)):
            self.do_loopback(packet_lengths[i], priorities[i])
            time.sleep(0.5)
        sniffer.join()
        print sniffer.results
        app_packets = 0
        for resp in sniffer.results:
            self.assertIsNotNone(resp)
            try:
                _ = resp[LNIC].dst
            except IndexError:
                continue
            if resp[LNIC].dst == MY_CONTEXT:
                resp_data = resp[LNIC].payload
                latency = struct.unpack('!Q', str(resp_data)[-8:])[0]
                self.assertTrue(resp[LNIC].src == LOW or resp[LNIC].src == HIGH)
                if resp[LNIC].src == LOW:
                    latency_low.append(latency)
                else:
                    latency_high.append(latency)
                app_packets += 1
        print "Low priority latencies: " + str(latency_low)
        print "Low count {}, avg {}".format(len(latency_low), sum(latency_low)/float(len(latency_low)))
        print "\nHigh priority latencies: " + str(latency_high)
        print "High count {}, avg {}".format(len(latency_high), sum(latency_high)/float(len(latency_high)))
        self.assertEqual(app_packets, len(packet_lengths))

class Loopback(unittest.TestCase):
    def do_loopback(self, pkt_len):
        msg_len = pkt_len - len(lnic_req()) # bytes
        payload = Raw('\x00'*msg_len)
        req = lnic_req() / payload
        # send request / receive response
        resp = srp1(req, iface=TEST_IFACE, timeout=TIMEOUT_SEC)
        self.assertIsNotNone(resp)
        self.assertEqual(resp[LNIC].dst, MY_CONTEXT)
        resp_data = resp[LNIC].payload
        self.assertEqual(len(resp_data), len(payload))
        latency = struct.unpack('!Q', str(resp_data)[-8:])[0]
        return latency
    def test_single(self):
        pkt_len = 64 # bytes
        latency = self.do_loopback(pkt_len)
        print 'Latency = {} cycles'.format(latency)
    def test_pkt_length(self):
        pkt_len = range(64, 64*20, 64)
        latency = []
        for l in pkt_len:
            print 'Testing pkt_len = {} bytes'.format(l)
            latency.append(self.do_loopback(l))
        # record latencies
        df = pd.DataFrame({'pkt_len':pkt_len, 'latency':latency})
        write_csv('loopback', 'pkt_len_latency.csv', df)

class Stream(unittest.TestCase):
    def do_loopback(self, pkt_len):
        msg_len = pkt_len - len(lnic_req()) # bytes
        payload = Raw('\x00'*msg_len)
        req = lnic_req() / payload
        # send request / receive response
        resp = srp1(req, iface=TEST_IFACE, timeout=TIMEOUT_SEC)
        self.assertIsNotNone(resp)
        self.assertEqual(resp[LNIC].dst, MY_CONTEXT)
        resp_data = resp[LNIC].payload
        self.assertEqual(len(resp_data), len(payload))
        latency = struct.unpack('!Q', str(resp_data)[-8:])[0]
        return latency
    def test_single(self):
        pkt_len = 64*2 # bytes
        latency = self.do_loopback(pkt_len)
        print 'Latency = {} cycles'.format(latency)
    def test_pkt_length(self):
        pkt_len = range(64, 64*15, 64)
        latency = []
        for l in pkt_len:
            print 'Testing pkt_len = {} bytes'.format(l)
            latency.append(self.do_loopback(l))
        # record latencies
        df = pd.DataFrame({'pkt_len':pkt_len, 'latency':latency})
        write_csv('stream', 'pkt_len_latency.csv', df)

class NNInference(unittest.TestCase):
    def setUp(self):
        bind_layers(LNIC, NN.NN)

    @staticmethod
    def config_msg(num_edges):
        return lnic_req() / NN.NN() / NN.Config(num_edges=num_edges)

    @staticmethod
    def weight_msg(index, weight):
        return lnic_req() / NN.NN() / NN.Weight(index=index, weight=weight)

    @staticmethod
    def data_msg(index, data):
        return lnic_req() / NN.NN() / NN.Data(index=index, data=data)

    def do_test(self, num_edges):
        inputs = []
        inputs.append(NNInference.config_msg(num_edges))
        # weight = 1 for all edges
        inputs += [NNInference.weight_msg(i, 1) for i in range(num_edges)]
        # data = index+1
        inputs += [NNInference.data_msg(i, i+1) for i in range(num_edges)]
        # send inputs get response
        resp = srp1(inputs, iface=TEST_IFACE, timeout=TIMEOUT_SEC)
        # check response
        self.assertIsNotNone(resp)
        self.assertTrue(resp.haslayer(NN.Data))
        self.assertEqual(resp[NN.Data].index, 0)
        self.assertEqual(resp[NN.Data].data, sum([i+1 for i in range(num_edges)]))
        # return latency
        return resp[NN.Data].timestamp

    def test_basic(self):
        latency = self.do_test(3)
        print 'Latency = {} cycles'.format(latency)

    def test_num_edges(self):
        num_edges = range(2, 10)
        latency = []
        for n in num_edges:
            latency.append(self.do_test(n))
        # record latencies
        df = pd.DataFrame({'num_edges':num_edges, 'latency':latency})
        write_csv('nn', 'num_edges_latency.csv', df)

class OthelloTest(unittest.TestCase):
    def setUp(self):
        bind_layers(LNIC, Othello.Othello)

    @staticmethod
    def map_msg(board, max_depth, cur_depth, src_host_id, src_msg_ptr):
        return lnic_req() / Othello.Othello() / \
                Othello.Map(board=board, max_depth=max_depth, cur_depth=cur_depth, src_host_id=src_host_id, src_msg_ptr=src_msg_ptr)

    @staticmethod
    def reduce_msg(target_host_id, target_msg_ptr, minimax_val):
        return lnic_req() / Othello.Othello() / \
                Othello.Reduce(target_host_id=target_host_id, target_msg_ptr=target_msg_ptr, minimax_val=minimax_val)

    def do_internal_node_test(self, fanout):
        # send in initial map msg and receive outgoing map messages
        parent_id = 10
        parent_msg_ptr = 0x1234
        responses = srp(OthelloTest.map_msg(board=fanout, max_depth=2, cur_depth=1, src_host_id=parent_id, src_msg_ptr=parent_msg_ptr),
                iface=TEST_IFACE, timeout=TIMEOUT_SEC, multi=True)
        # check responses / build reduce msgs
        self.assertEqual(len(responses[0]), fanout)
        reduce_msgs = []
        map_latency = None
        for _, p in responses[0]:
            self.assertTrue(p.haslayer(Othello.Map))
            self.assertEqual(p[Othello.Map].cur_depth, 2)
            reduce_msgs.append(OthelloTest.reduce_msg(
                target_host_id=p[Othello.Map].src_host_id,
                target_msg_ptr=p[Othello.Map].src_msg_ptr,
                minimax_val=1))
            map_latency = p[Othello.Map].timestamp
        # send in reduce messages and receive final reduce msg
        resp = srp1(reduce_msgs, iface=TEST_IFACE, timeout=TIMEOUT_SEC)
        # check reduce msg responses
        self.assertIsNotNone(resp)
        self.assertTrue(resp.haslayer(Othello.Reduce))
        self.assertEqual(resp[Othello.Reduce].target_host_id, parent_id)
        self.assertEqual(resp[Othello.Reduce].target_msg_ptr, parent_msg_ptr)
        self.assertEqual(resp[Othello.Reduce].minimax_val, 1)
        reduce_latency = resp[Othello.Reduce].timestamp
        return map_latency, reduce_latency

    def test_internal_node_basic(self):
        map_latency, reduce_latency = self.do_internal_node_test(3)
        print 'Map Latency = {} cycles'.format(map_latency)
        print 'Reduce Latency = {} cycles'.format(reduce_latency)

    def test_fanout(self):
        fanout = range(2, 10)
        map_latency = []
        reduce_latency = []
        for n in fanout:
            mlat, rlat = self.do_internal_node_test(n)
            map_latency.append(mlat)
            reduce_latency.append(rlat)
        # record latencies
        df = pd.DataFrame({'fanout':fanout, 'map_latency':map_latency, 'reduce_latency':reduce_latency})
        write_csv('othello', 'fanout_latency.csv', df)


class Multithread(unittest.TestCase):
    def test_basic(self):
        pkt_len = 64 # bytes
        msg_len = pkt_len - len(lnic_req()) # bytes
        payload = Raw('\x00'*msg_len)
        sendp(lnic_req(1) / payload, iface=TEST_IFACE)
        sendp(lnic_req(0) / payload, iface=TEST_IFACE)
        self.assertTrue(True)

