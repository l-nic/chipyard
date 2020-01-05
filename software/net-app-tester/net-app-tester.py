
import unittest
from scapy.all import *
from LNIC_headers import LNIC
import NN_headers as NN
import Othello_headers as Othello
import struct

TEST_IFACE = "tap0"
TIMEOUT_SEC = 2 # seconds

NIC_MAC = "08:11:22:33:44:08"
MY_MAC = "08:55:66:77:88:08"

NIC_IP = "10.0.0.1"
MY_IP = "10.1.2.3"

DST_CONTEXT = 0
MY_CONTEXT = 0x1234

def lnic_req():
    return Ether(dst=NIC_MAC, src=MY_MAC) / \
            IP(src=MY_IP, dst=NIC_IP) / \
            LNIC(src=MY_CONTEXT, dst=DST_CONTEXT)

# Test to check basic loopback functionality
class SimpleLoopback(unittest.TestCase):
    def test_loopback(self):
        msg_len = 16 # bytes
        payload = Raw('\x00'*msg_len)
        req = lnic_req() / payload
        print "---------- Request ({} B) -------------".format(len(req))
        req.show2()
        # send request / receive response
        resp = srp1(req, iface=TEST_IFACE, timeout=TIMEOUT_SEC)
        self.assertIsNotNone(resp)
        print "---------- Response ({} B) -------------".format(len(resp))
        resp.show2()
        msg_data = resp[LNIC].payload
        self.assertEqual(len(msg_data), len(payload))
        latency = struct.unpack('!Q', str(msg_data)[-8:])[0]
        print 'Latency = {} cycles'.format(latency)

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

    def test_basic(self):
        resp = srp1([NNInference.config_msg(3),
            NNInference.weight_msg(0, 1),
            NNInference.weight_msg(1, 1),
            NNInference.weight_msg(2, 1),
            NNInference.data_msg(0, 1),
            NNInference.data_msg(1, 2),
            NNInference.data_msg(2, 3)], iface=TEST_IFACE, timeout=TIMEOUT_SEC)

        # check response
        self.assertIsNotNone(resp)
        print '------------- Response -------------'
        resp.show2()
        self.assertTrue(resp.haslayer(NN.Data))
        self.assertEqual(resp[NN.Data].index, 0)
        self.assertEqual(resp[NN.Data].data, 6)
        print 'Latency = {} cycles'.format(resp[NN.Data].timestamp)

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

    def test_internal_node(self):
        # send in initial map msg and receive outgoing map messages
        parent_id = 10
        parent_msg_ptr = 0x1234
        responses = srp(OthelloTest.map_msg(board=0, max_depth=2, cur_depth=1, src_host_id=parent_id, src_msg_ptr=parent_msg_ptr),
                iface=TEST_IFACE, timeout=TIMEOUT_SEC, multi=True)
        # check responses / build reduce msgs
        self.assertEqual(len(responses[0]), 2)
        reduce_msgs = []
        map_latency = None
        print '------------ Map Responses -----------'
        for _, p in responses[0]:
            p.show()
            self.assertTrue(p.haslayer(Othello.Map))
            self.assertEqual(p[Othello.Map].cur_depth, 2)
            reduce_msgs.append(OthelloTest.reduce_msg(
                target_host_id=p[Othello.Map].src_host_id,
                target_msg_ptr=p[Othello.Map].src_msg_ptr,
                minimax_val=1))
            map_latency = p[Othello.Map].timestamp
        print 'Map Latency = {} cycles'.format(map_latency)
        # send in reduce messages and receive final reduce msg
        resp = srp1(reduce_msgs, iface=TEST_IFACE, timeout=TIMEOUT_SEC)
        # check reduce msg responses
        self.assertIsNotNone(resp)
        print '----------- Reduce Response ------------'
        resp.show()
        self.assertTrue(resp.haslayer(Othello.Reduce))
        self.assertEqual(resp[Othello.Reduce].target_host_id, parent_id)
        self.assertEqual(resp[Othello.Reduce].target_msg_ptr, parent_msg_ptr)
        self.assertEqual(resp[Othello.Reduce].minimax_val, 1)
        print 'Reduce Latency = {} cycles'.format(resp[Othello.Reduce].timestamp)

