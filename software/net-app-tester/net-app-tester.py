
import unittest
from scapy.all import *
from LNIC_headers import LNIC
import NN_headers as NN
import Othello_headers as Othello
import struct
import pandas as pd
import os
import threading
import traceback
import time

TEST_IFACE = "tap0"
TIMEOUT_SEC = 6 # seconds

NIC_MAC = "08:11:22:33:44:08"
MY_MAC = "08:55:66:77:88:08"

NIC_IP = "10.0.0.1"
MY_IP = "10.1.2.3"

DST_CONTEXT = 0
MY_CONTEXT = 0x1235

LOG_DIR = '/vagrant/logs'

def lnic_req(my_context=MY_CONTEXT, dst_context=DST_CONTEXT):
    return Ether(dst=NIC_MAC, src=MY_MAC) / \
            IP(src=MY_IP, dst=NIC_IP) / \
            LNIC(src=my_context, dst=dst_context)

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
        sniffer = AsyncSniffer(iface=TEST_IFACE, timeout=10)#len(contexts)*len(packet_lengths))
        sniffer.start()
        for pkt_len in packet_lengths:
            for my_context, dst_context in contexts:
                msg_len = pkt_len - len(lnic_req()) # bytes
                payload = Raw('\x00'*msg_len)
                req = lnic_req(my_context=my_context, dst_context=dst_context) / payload
                sendp(req, iface=TEST_IFACE)
                time.sleep(.1)
        sniffer.join()
        # print sniffer.results
        real_packets = 0
        for resp in sniffer.results:
            self.assertIsNotNone(resp)
            if resp[LNIC].dst == 0x1235 or resp[LNIC].dst == 0x1236:
                resp_data = resp[LNIC].payload
                resp_value = struct.unpack('!Q', str(resp_data)[:8])[0]
                print "Packet from context id " + str(resp[LNIC].src) + " contains value " + str(resp_value)
                real_packets += 1
        print "Total packets received: " + str(real_packets)
        return

        # Start the sender threads and the packet sniffer
        exc0 = []
        exc1 = []
        context0_thread = threading.Thread(target=self.context0, args=(exc0, pkt_len))
        context1_thread = threading.Thread(target=self.context1, args=(exc1, pkt_len))
        sniffer = AsyncSniffer(iface=TEST_IFACE, count=2*num_packets)
        sniffer.start()
        context0_thread.start()
        context1_thread.start()
        context0_thread.join()
        context1_thread.join()
        print "All data sent"

        # Handle exceptions in the sender threads
        if len(exc0) > 0 or len(exc1) > 0:
            if len(exc0) > 0:
                print exc0[0]
            if len(exc1) > 0:
                print exc1[0]
            raise Exception("Context thread encountered exception")
        
        # Wait for all packets to be received, and print the results
        sniffer.join()
        print sniffer.results
        for resp in sniffer.results:
            print(hexdump(resp))
            self.assertIsNotNone(resp)
            print "Dst id: " + str(resp[LNIC].dst)
            resp_data = resp[LNIC].payload
            print "Data: " + str(struct.unpack('!Q', str(resp_data)[:8])[0])
        #print t.stop()

    def context0(self, exc, pkt_len):
        try:
            my_context = 0x1235
            dst_context = 0
            self.send_packets(my_context, dst_context, pkt_len)
        except Exception as e:
            exc.append(traceback.format_exc())

    def context1(self, exc, pkt_len):
        try:
            my_context = 0x1236
            dst_context = 1
            self.send_packets(my_context, dst_context, pkt_len)
        except Exception as e:
            exc.append(traceback.format_exc())

    def send_packets(self, my_context, dst_context, pkt_len):
        for l in pkt_len:
            print 'Testing pkt_len = {} bytes'.format(l)
            self.do_loopback(my_context, dst_context, l)

    def do_loopback(self, my_context, dst_context, pkt_len):
        msg_len = pkt_len - len(lnic_req()) # bytes
        payload = Raw('\x00'*msg_len)
        req = lnic_req(my_context=my_context, dst_context=dst_context) / payload
        # send request
        sendp(req, iface=TEST_IFACE)

class Loopback(unittest.TestCase):
    def do_loopback(self, pkt_len):
        msg_len = pkt_len - len(lnic_req()) # bytes
        payload = Raw('\x00'*msg_len)
        req = lnic_req(dst_context=1) / payload
        #self.prev_context = not self.prev_context
        # send request / receive response
        resp = srp1(req, iface=TEST_IFACE, timeout=TIMEOUT_SEC)
        self.assertIsNotNone(resp)
        self.assertEqual(resp[LNIC].dst, MY_CONTEXT or 0x192)
        resp_data = resp[LNIC].payload
        self.assertEqual(len(resp_data), len(payload))
        # latency = struct.unpack('!Q', str(resp_data)[-8:])[0]
        return struct.unpack('!Q', str(resp_data)[:8])[0]
    def test_single(self):
        pkt_len = 64 # bytes
        returned_word = self.do_loopback(pkt_len)
        #print 'Latency = {} cycles'.format(latency)
        print "Returned word: " + str(returned_word)
    def test_pkt_length(self):
        pkt_len = range(64, 64*20, 64)
        returned_words = []
        for l in pkt_len:
            print 'Testing pkt_len = {} bytes'.format(l)
            returned_words.append(self.do_loopback(l))
        print returned_words
        # record latencies
        #df = pd.DataFrame({'pkt_len':pkt_len, 'latency':latency})
        #write_csv('loopback', 'pkt_len_latency.csv', df)

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

