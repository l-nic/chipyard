
import unittest
from scapy.all import *
from LNIC_headers import LNIC
from LNIC_utils import *
import Throughput_headers as Throughput
import NN_headers as NN
import Othello_headers as Othello
import NBody_headers as NBody
import DummyApp_headers as DummyApp
from ChainRep_headers import ChainRep, CHAINREP_OP_READ, CHAINREP_OP_WRITE
import struct
import pandas as pd
import os
import random
import numpy as np

# set random seed for consistent sims
random.seed(1)
np.random.seed(1)

TEST_IFACE = "tap0"
TIMEOUT_SEC = 7 # seconds

NIC_MAC = "08:11:22:33:44:08"
MY_MAC = "08:55:66:77:88:08"

NIC_IP = "10.0.0.1"
MY_IP = "10.0.0.3"

DST_CONTEXT = 0
LATENCY_CONTEXT = 0x1234 # use this when we want the HW to insert timestamps into DATA pkts
DEFAULT_CONTEXT = 0x5678

LOG_DIR = '/vagrant/logs'

NUM_SAMPLES = 1

def lnic_pkt(msg_len, pkt_offset, src_context=DEFAULT_CONTEXT, dst_context=DST_CONTEXT, src_ip=MY_IP):
    return Ether(dst=NIC_MAC, src=MY_MAC) / \
            IP(src=src_ip, dst=NIC_IP) / \
            LNIC(flags='DATA', src_context=src_context, dst_context=dst_context, msg_len=msg_len, pkt_offset=pkt_offset)

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
      hexdump(pkts[i])

def packetize(msg, src_context=DEFAULT_CONTEXT, dst_context=DST_CONTEXT, src_ip=MY_IP):
    """Generate LNIC pkts for the given msg
    """
    num_pkts = compute_num_pkts(len(msg))
    pkts = []
    for i in range(num_pkts-1):
        p = lnic_pkt(len(msg), i, src_context, dst_context, src_ip) / Raw(msg[i*MAX_SEG_LEN_BYTES:(i+1)*MAX_SEG_LEN_BYTES])
        pkts.append(p)
    p = lnic_pkt(len(msg), num_pkts-1, src_context, dst_context, src_ip) / Raw(msg[(num_pkts-1)*MAX_SEG_LEN_BYTES:])
    pkts.append(p)
    return pkts

class SchedulerTest(unittest.TestCase):
    def setUp(self):
        bind_layers(LNIC, DummyApp.DummyApp)

    def app_msg(self, priority, service_time, pkt_len):
        msg_len = pkt_len - len(Ether()/IP()/LNIC())
        return lnic_pkt(msg_len, 0, src_context=0, dst_context=priority) / DummyApp.DummyApp(service_time=service_time) / \
               Raw('\x00'*(pkt_len - len(Ether()/IP()/LNIC()/DummyApp.DummyApp())))

    def test_scheduler(self):
        num_lp_msgs = 9
        num_hp_msgs = 9
        service_time = 500
        inputs = []
        # add high priority msgs
        inputs += [self.app_msg(0, service_time, 128) for i in range(num_hp_msgs)]
        # add low priority msgs 
        inputs += [self.app_msg(1, service_time, 128) for i in range(num_lp_msgs)]
        # shuffle pkts
        random.shuffle(inputs)

        receiver = LNICReceiver(TEST_IFACE)
        # start sniffing for responses
        sniffer = AsyncSniffer(iface=TEST_IFACE, lfilter=lambda x: x.haslayer(LNIC) and x[LNIC].flags.DATA and x[LNIC].dst_context == LATENCY_CONTEXT,
                    prn=receiver.process_pkt, count=num_lp_msgs + num_hp_msgs, timeout=100)
        sniffer.start()
        # send in pkts
        sendp(inputs, iface=TEST_IFACE, inter=1.1)
        # wait for all responses
        sniffer.join()
        # check responses
        self.assertEqual(len(sniffer.results), num_lp_msgs + num_hp_msgs)
        time = []
        context = []
        latency = []
        for p in sniffer.results:
            self.assertTrue(p.haslayer(LNIC))
            l = struct.unpack('!L', str(p)[-4:])[0]
            t = struct.unpack('!L', str(p)[-8:-4])[0]
            self.assertTrue(p[LNIC].src_context in [0, 1])
            time.append(t)
            context.append(p[LNIC].src_context)
            latency.append(l)
        # record latencies in a DataFrame
        df = pd.DataFrame({'time': pd.Series(time), 'context': pd.Series(context), 'latency': pd.Series(latency)}, dtype=float)
        print df
        write_csv('scheduler', 'stats.csv', df)

class LoadBalanceTest(unittest.TestCase):
    def setUp(self):
        bind_layers(LNIC, DummyApp.DummyApp)

    def app_msg(self, dst_context, service_time, pkt_len):
        msg_len = pkt_len - len(Ether()/IP()/LNIC())
        return lnic_pkt(msg_len, 0, src_context=LATENCY_CONTEXT, dst_context=dst_context) / DummyApp.DummyApp(service_time=service_time) / \
               Raw('\x00'*(pkt_len - len(Ether()/IP()/LNIC()/DummyApp.DummyApp())))

    def test_load_balance(self):
        num_msgs = 50
        num_contexts = 1
        inputs = []
        mean_service_times = [500]*3 + [3000]
        # create msgs
        for i in range(num_msgs):
            ctx = random.randint(0, num_contexts-1)
            service_time = random.choice(mean_service_times) # int(np.random.exponential(mean_service_time))
            p = self.app_msg(ctx, service_time, 128)
            inputs.append(p)

        receiver = LNICReceiver(TEST_IFACE)
        # start sniffing for responses
        sniffer = AsyncSniffer(iface=TEST_IFACE, lfilter=lambda x: x.haslayer(LNIC) and x[LNIC].flags.DATA and x[LNIC].dst_context == LATENCY_CONTEXT,
                    prn=receiver.process_pkt, count=num_msgs, timeout=200)
        sniffer.start()
        # send in pkts
        sendp(inputs, iface=TEST_IFACE, inter=1.2)
        # wait for all responses
        sniffer.join()
        # check responses
        self.assertEqual(len(sniffer.results), num_msgs)
        time = []
        context = []
        latency = []
        service_time = []
        for p in sniffer.results:
            self.assertTrue(p.haslayer(LNIC))
            s = p[DummyApp.DummyApp].service_time
            l = struct.unpack('!L', str(p)[-4:])[0]
            t = struct.unpack('!L', str(p)[-8:-4])[0]
            self.assertTrue(p[LNIC].src_context < num_contexts)
            time.append(t)
            context.append(p[LNIC].src_context)
            latency.append(l)
            service_time.append(s)
        # record latencies in a DataFrame
        df = pd.DataFrame({'time': pd.Series(time), 'context': pd.Series(context), 'latency': pd.Series(latency), 'service_time': pd.Series(service_time)}, dtype=float)
        print df
        write_csv('load-balance', 'stats.csv', df)

class Mica(unittest.TestCase):
    def do_loopback(self, pkts):
        print "*********** Request Pkts: ***********"
        print_pkts(pkts)
        # send request pkts / receive response pkts
        receiver = LNICReceiver(TEST_IFACE)
        # start sniffing for responses
        sniffer = AsyncSniffer(iface=TEST_IFACE, lfilter=lambda x: x.haslayer(LNIC) and x[LNIC].flags.DATA and x[LNIC].dst_context == DEFAULT_CONTEXT,
                    prn=receiver.process_pkt, count=len(pkts), timeout=10)
        sniffer.start()
        # send in pkts
        sendp(pkts, iface=TEST_IFACE)
        # wait for all response pkts
        sniffer.join()
        self.assertEqual(len(pkts), len(sniffer.results))
        print "*********** Response Pkts: ***********"
        print_pkts(sniffer.results)
        return receiver.msgs
    def test_read(self):
        msg_type_r = '\x00\x00\x00\x00\x00\x00\x00\x01'
        msg_key    = '\x00\x00\x00\x00\x00\x00\x00\x02'
        payload = Raw(msg_type_r + msg_key)
        dst_ctx = DST_CONTEXT
        pkts = packetize(str(payload), DEFAULT_CONTEXT, dst_ctx)
        rx_msgs = self.do_loopback(pkts)
    def test_write(self):
        msg_type_w = '\x00\x00\x00\x00\x00\x00\x00\x02'
        msg_key    = '\x00\x00\x00\x00\x00\x00\x00\x02'
        msg_val    = '\x00\x00\x00\x00\x00\x00\x00\x07'
        payload = Raw(msg_type_w + msg_key + msg_val)
        dst_ctx = DST_CONTEXT
        pkts = packetize(str(payload), DEFAULT_CONTEXT, dst_ctx)
        rx_msgs = self.do_loopback(pkts)

class ChainReplication(unittest.TestCase):
    def setUp(self):
        bind_layers(LNIC, ChainRep)

    @staticmethod
    def read_msg(nodes=[NIC_IP], dst_context=DST_CONTEXT, key=0):
        cr_hdr = ChainRep(nodes=nodes[1:], client_ip=MY_IP, op=CHAINREP_OP_READ, seq=0, key=key, value=0)
        return Ether(dst=NIC_MAC, src=MY_MAC) / \
                IP(src=MY_IP, dst=nodes[0]) / \
                LNIC(flags='DATA', src_context=DEFAULT_CONTEXT, dst_context=dst_context, msg_len=len(cr_hdr), pkt_offset=0) / cr_hdr
    @staticmethod
    def write_msg(nodes=[NIC_IP], dst_context=DST_CONTEXT, seq=0, key=0, val=0):
        cr_hdr = ChainRep(nodes=nodes[1:], client_ip=MY_IP, op=CHAINREP_OP_WRITE, seq=seq, key=key, value=val)
        return Ether(dst=NIC_MAC, src=MY_MAC) / \
                IP(src=MY_IP, dst=nodes[0]) / \
                LNIC(flags='DATA', src_context=DEFAULT_CONTEXT, dst_context=dst_context, msg_len=len(cr_hdr), pkt_offset=0) / cr_hdr

    def stop_filter(self, p):
        return p[IP].dst == MY_IP

    def fwd_pkt(self, p):
        self.assertTrue(p.haslayer(ChainRep))
        if p[IP].dst == MY_IP: return
        resp = p.copy()
        resp[Ether].src = p[Ether].dst
        resp[Ether].dst = p[Ether].src
        resp[LNIC].dst_context = DST_CONTEXT
        resp[LNIC].src_context = DEFAULT_CONTEXT
        sendp([resp], iface=TEST_IFACE)

    def test_write(self):
        receiver = LNICReceiver(TEST_IFACE, prn=self.fwd_pkt)
        sniffer = AsyncSniffer(iface=TEST_IFACE, lfilter=lambda x: x.haslayer(LNIC) and x[LNIC].flags.DATA and x[Ether].src != MY_MAC,
                    prn=receiver.process_pkt, stop_filter=self.stop_filter, timeout=100)
        sniffer.start()
        nodes = ['10.0.0.3', '10.0.0.4', '10.0.0.5']
        req = ChainReplication.write_msg(nodes=nodes, key=4, val=7, seq=0)
        sendp([req], iface=TEST_IFACE)
        sniffer.join()
        self.assertEqual(len(sniffer.results), 3)
        for i,resp in enumerate(sniffer.results):
            self.assertEqual(resp[ChainRep].op, CHAINREP_OP_WRITE)
            self.assertEqual(resp[ChainRep].key, req[ChainRep].key)
            self.assertEqual(resp[ChainRep].value, req[ChainRep].value)
            self.assertEqual(resp[ChainRep].seq, req[ChainRep].seq)
            self.assertEqual(resp[ChainRep].node_cnt, max(len(nodes)-2-i, 0))
        p1, p2, p3 = sniffer.results
        self.assertEqual(p1[ChainRep].nodes, nodes[2:])
        self.assertEqual(p1[IP].dst, nodes[1])
        self.assertEqual(p2[IP].dst, nodes[2])
        self.assertEqual(p3[IP].dst, req[ChainRep].client_ip)

    def test_read(self):
        receiver = LNICReceiver(TEST_IFACE, prn=self.fwd_pkt)
        sniffer = AsyncSniffer(iface=TEST_IFACE, lfilter=lambda x: x.haslayer(LNIC) and x[LNIC].flags.DATA and x[Ether].src != MY_MAC,
                    prn=receiver.process_pkt, stop_filter=self.stop_filter, timeout=100)
        sniffer.start()
        req = ChainReplication.read_msg(nodes=[NIC_IP], key=3)
        sendp([req], iface=TEST_IFACE)
        sniffer.join()
        self.assertEqual(len(sniffer.results), 1)
        resp = sniffer.results[0]
        self.assertEqual(resp[IP].dst, MY_IP)
        self.assertEqual(resp[ChainRep].key, 3)
        self.assertEqual(resp[ChainRep].value, 0)

class Loopback(unittest.TestCase):
    def do_loopback(self, pkts):
#        print "*********** Request Pkts: ***********"
#        print_pkts(pkts)
        # send request pkts / receive response pkts
        receiver = LNICReceiver(TEST_IFACE)
        # start sniffing for responses
        sniffer = AsyncSniffer(iface=TEST_IFACE, lfilter=lambda x: x.haslayer(IP) and x.haslayer(LNIC) and x[LNIC].flags.DATA and x[IP].src == NIC_IP,
                    prn=receiver.process_pkt, count=len(pkts), timeout=100)
        sniffer.start()
        # send in pkts
        sendp(pkts, iface=TEST_IFACE)
        # wait for all response pkts
        sniffer.join()
        self.assertEqual(len(pkts), len(sniffer.results))
#        print "*********** Response Pkts: ***********"
#        print_pkts(sniffer.results)
        return receiver.msgs
    def test_multi_host(self):
        num_hosts = 32
        src_ips = ['10.0.0.{}'.format(i) for i in range(2, 2 + num_hosts)]
        src_contexts = range(num_hosts)
        tx_msgs = {}
        pkts = []
        for src_ip, src_context in zip(src_ips, src_contexts):
            num_words = random.randint(1, 256)
            msg = ''.join(['{:0>8}'.format(x) for x in range(num_words)])
            tx_msgs[(src_ip, src_context)] = msg
            pkts += packetize(msg, src_context, DST_CONTEXT, src_ip)
        random.shuffle(pkts)
        rx_msgs = self.do_loopback(pkts)
        self.assertEqual(len(src_ips), len(rx_msgs))
        for ip, context in zip(src_ips, src_contexts):
            self.check_msg(rx_msgs, ip, context, NIC_IP, DST_CONTEXT, tx_msgs[(ip, context)])
    def check_msg(self, rx_msgs, dst_ip, dst_context, src_ip, src_context, msg):
        for m in rx_msgs:
            if m[0][0] == dst_ip and m[0][1] == dst_context and m[0][2] == src_ip and m[0][3] == src_context:
                if msg != m[1]:
                    print "ERROR: Incorrect msg for host: {}".format(dst_ip)
                    print "rx_msgs:"
                    for rx_msg in rx_msgs:
                        print "----------------------{}----------------------".format(rx_msg[0][0])
                        print "{}".format(rx_msg[1])
                self.assertEqual(msg, m[1])
                return
        self.assertTrue(False, "Could not find expected msg!")

#    def test_pkt_length(self):
#        pkt_len = range(64, 64*20, 64)
#        length = []
#        latency = []
#        for l in pkt_len:
#            for i in range(NUM_SAMPLES):
#                print 'Testing pkt_len = {} bytes'.format(l)
#                length.append(l)
#                latency.append(self.do_loopback(l))
#        # record latencies
#        df = pd.DataFrame({'pkt_len':length, 'latency':latency})
#        write_csv('loopback', 'pkt_len_latency.csv', df)

class LoopbackLatency(unittest.TestCase):
    def do_loopback(self, pkts):
#        print "*********** Request Pkts: ***********"
#        print_pkts(pkts)
        # send request pkts / receive response pkts
        receiver = LNICReceiver(TEST_IFACE)
        # start sniffing for responses
        sniffer = AsyncSniffer(iface=TEST_IFACE, lfilter=lambda x: x.haslayer(IP) and x.haslayer(LNIC) and x[LNIC].flags.DATA and x[IP].src == NIC_IP,
                    prn=receiver.process_pkt, count=len(pkts), timeout=100)
        sniffer.start()
        # send in pkts
        sendp(pkts, iface=TEST_IFACE)
        # wait for all response pkts
        sniffer.join()
        self.assertEqual(len(pkts), len(sniffer.results))
#        print "*********** Response Pkts: ***********"
#        print_pkts(sniffer.results)
        return sniffer.results
    def test_latency(self):
        msg_len = 8 # bytes
        pkts = [lnic_pkt(msg_len, 0, src_context=LATENCY_CONTEXT, dst_context=0) / Raw('\x00'*msg_len)]
        rx_pkts = self.do_loopback(pkts)
        self.assertEqual(1, len(rx_pkts))
        p = rx_pkts[0]
        latency = struct.unpack('!L', str(p)[-4:])[0]
        time = struct.unpack('!L', str(p)[-8:-4])[0]
        print "latency = {} cycles".format(latency)

class ThroughputTest(unittest.TestCase):
    def setUp(self):
        bind_layers(LNIC, Throughput.Throughput)

    def start_rx_msg(self, num_msgs):
        msg = Throughput.Throughput() / Throughput.StartRx(num_msgs=num_msgs) / Raw('\x00'*8)
        return lnic_pkt(len(msg), 0, src_context=LATENCY_CONTEXT, dst_context=0) / msg

    def start_tx_msg(self, num_msgs, msg_size):
        msg = Throughput.Throughput() / Throughput.StartTx(num_msgs=num_msgs, msg_size=msg_size) / Raw('\x00'*8)
        return lnic_pkt(len(msg), 0, src_context=LATENCY_CONTEXT, dst_context=0) / msg

    def data_msg(self, msg_len):
        msg = Throughput.Throughput(msg_type=Throughput.DATA_TYPE) / Raw('\x00'*(msg_len - len(Throughput.Throughput())))
        return lnic_pkt(len(msg), 0, src_context=LATENCY_CONTEXT, dst_context=0) / msg

    def do_rx_test(self, num_msgs, msg_len):
        # test RX throughput - how fast can the application receive msgs?
        pkts = []
        pkts += [self.start_rx_msg(num_msgs)]
        for i in range(num_msgs):
            pkts += [self.data_msg(msg_len)]
        receiver = LNICReceiver(TEST_IFACE)
        # start sniffing for DONE msg
        sniffer = AsyncSniffer(iface=TEST_IFACE, lfilter=lambda x: x.haslayer(IP) and x.haslayer(LNIC) and x[LNIC].flags.DATA and x[IP].src == NIC_IP,
                    prn=receiver.process_pkt, count=1, timeout=100)
        sniffer.start()
        # send in pkts
        sendp(pkts, iface=TEST_IFACE)
        # wait for DONE msg
        sniffer.join()
        self.assertEqual(1, len(sniffer.results))
        done_msg = sniffer.results[0]
        total_latency = struct.unpack('!L', str(done_msg)[-4:])[0]
        total_bytes = (len(Ether()/IP()/LNIC()) + msg_len)*num_msgs
        throughput = total_bytes/float(total_latency)
        return throughput # bytes/cycle

    def do_tx_test(self, num_msgs, msg_len):
        # test TX throughput - how fast can the application generate pkts?
        start_msg = self.start_tx_msg(num_msgs, msg_len)
        receiver = LNICReceiver(TEST_IFACE)
        # start sniffing for generated msgs
        sniffer = AsyncSniffer(iface=TEST_IFACE, lfilter=lambda x: x.haslayer(IP) and x.haslayer(LNIC) and x[LNIC].flags.DATA and x[IP].src == NIC_IP,
                    prn=receiver.process_pkt, count=num_msgs, timeout=100)
        sniffer.start()
        # send in START msg
        sendp(start_msg, iface=TEST_IFACE)
        # wait for all generated DATA msgs
        sniffer.join()
        self.assertEqual(num_msgs, len(sniffer.results))
        for p in sniffer.results:
            self.assertEqual(len(Ether()/IP()/LNIC()) + msg_len, len(p))
        final_msg = sniffer.results[-1]
        total_latency = struct.unpack('!L', str(final_msg)[-4:])[0]
        total_bytes = reduce(lambda a,b: a+b, map(len, sniffer.results))
        throughput = total_bytes/float(total_latency)
        print 'total_latency = {} cycles'.format(total_latency)
        print 'total_bytes = {} bytes'.format(total_bytes)
        return throughput # bytes/cycle

    def test_rx_throughput(self):
        msg_len = MAX_SEG_LEN_BYTES
        num_msgs = 100
        throughput = self.do_rx_test(num_msgs, msg_len)
        print 'RX Throughput = {} bytes/cycle ({} Gbps)'.format(throughput, throughput*8.0/0.3125)

    def test_tx_throughput(self):
        msg_len = MAX_SEG_LEN_BYTES
        num_msgs = 100
        throughput = self.do_tx_test(num_msgs, msg_len)
        print 'TX Throughput = {} bytes/cycle ({} Gbps)'.format(throughput, throughput*8.0/0.3125)

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
        length = []
        latency = []
        for l in pkt_len:
            for i in range(NUM_SAMPLES):
              print 'Testing pkt_len = {} bytes'.format(l)
              length.append(l)
              latency.append(self.do_loopback(l))
        # record latencies
        df = pd.DataFrame({'pkt_len':length, 'latency':latency})
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
        # start sniffing for DONE msg
        sniffer = AsyncSniffer(iface=TEST_IFACE, lfilter=lambda x: x.haslayer(LNIC) and x[LNIC].dst == MY_CONTEXT,
                    count=1, timeout=TIMEOUT_SEC)
        sniffer.start()
        # send inputs
        sendp(inputs, iface=TEST_IFACE)
        # wait for response
        sniffer.join()
        # check response
        self.assertEqual(len(sniffer.results), 1)
        resp = sniffer.results[0]
        self.assertTrue(resp.haslayer(NN.Data))
        self.assertEqual(resp[NN.Data].index, 0)
        self.assertEqual(resp[NN.Data].data, sum([i+1 for i in range(num_edges)]))
        # return latency
        return resp[NN.Data].timestamp

    def test_basic(self):
        latency = self.do_test(3)
        print 'Latency = {} cycles'.format(latency)

    def test_num_edges(self):
        num_edges = range(2, 21)
        edges = []
        latency = []
        for n in num_edges:
            for i in range(NUM_SAMPLES):
                edges.append(n)
                latency.append(self.do_test(n))
        # record latencies
        df = pd.DataFrame({'num_edges':edges, 'latency':latency})
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
        req = OthelloTest.map_msg(board=fanout, max_depth=2, cur_depth=1, src_host_id=parent_id, src_msg_ptr=parent_msg_ptr)
        # start sniffing for DONE msg
        sniffer = AsyncSniffer(iface=TEST_IFACE, lfilter=lambda x: x.haslayer(LNIC) and x[LNIC].dst == MY_CONTEXT,
                    count=fanout, timeout=TIMEOUT_SEC)
        sniffer.start()
        # send in the request
        sendp(req, iface=TEST_IFACE)
        # wait for all responses
        sniffer.join()
        # check responses / build reduce msgs
        self.assertEqual(len(sniffer.results), fanout)
        reduce_msgs = []
        map_latency = None
        for p in sniffer.results:
            self.assertTrue(p.haslayer(Othello.Map))
            self.assertEqual(p[Othello.Map].cur_depth, 2)
            reduce_msgs.append(OthelloTest.reduce_msg(
                target_host_id=p[Othello.Map].src_host_id,
                target_msg_ptr=p[Othello.Map].src_msg_ptr,
                minimax_val=1))
            map_latency = p[Othello.Map].timestamp
        # start sniffing for DONE msg
        sniffer = AsyncSniffer(iface=TEST_IFACE, lfilter=lambda x: x.haslayer(LNIC) and x[LNIC].dst == MY_CONTEXT,
                    count=1, timeout=TIMEOUT_SEC)
        sniffer.start()
        # send in reduce messages
        sendp(reduce_msgs, iface=TEST_IFACE)
        # wait for response
        sniffer.join()
        # check reduce msg responses
        self.assertEqual(len(sniffer.results), 1)
        resp = sniffer.results[0]
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
        fanout_vals = range(2, 10)
        fanout = []
        map_latency = []
        reduce_latency = []
        for n in fanout_vals:
            for i in range(NUM_SAMPLES):
                fanout.append(n)
                mlat, rlat = self.do_internal_node_test(n)
                map_latency.append(mlat)
                reduce_latency.append(rlat)
        # record latencies
        df = pd.DataFrame({'fanout':fanout, 'map_latency':map_latency, 'reduce_latency':reduce_latency})
        write_csv('othello', 'fanout_latency.csv', df)

class NBodyTest(unittest.TestCase):
    G = 667e2
    def setUp(self):
        bind_layers(LNIC, NBody.NBody)

    def config_msg(self, xcom, ycom, num_msgs):
        return lnic_req() / NBody.NBody() / NBody.Config(xcom=xcom, ycom=ycom, num_msgs=num_msgs)

    def traversal_req(self, xpos, ypos):
        return lnic_req() / NBody.NBody() / NBody.TraversalReq(xpos=xpos, ypos=ypos)

    def do_test(self, num_msgs):
        inputs = []
        xcom = 50
        ycom = 50
        xpos = 0
        ypos = 0
        inputs.append(self.config_msg(xcom, ycom, num_msgs))
        # generate traversal req msgs
        inputs += [self.traversal_req(xpos, ypos) for i in range(num_msgs)]
        # start sniffing for responses
        sniffer = AsyncSniffer(iface=TEST_IFACE, lfilter=lambda x: x.haslayer(LNIC) and x[LNIC].dst == MY_CONTEXT,
                    count=num_msgs, timeout=20)
        sniffer.start()
        # send inputs get response
        sendp(inputs, iface=TEST_IFACE)
        # wait for all responses
        sniffer.join()
        # check response
        self.assertEqual(len(sniffer.results), num_msgs)
        final_resp = sniffer.results[-1]
        self.assertTrue(final_resp.haslayer(NBody.TraversalResp))
        # compute expected force
        dist = np.sqrt((xcom - xpos)**2 + (ycom - ypos)**2)
        expected_force = NBodyTest.G / dist**2
        self.assertAlmostEqual(final_resp[NBody.TraversalResp].force, expected_force, delta=1)
        # return latency
        return final_resp[NBody.TraversalResp].timestamp

    def test_basic(self):
        latency = self.do_test(3)
        print 'Latency = {} cycles'.format(latency)

    def test_num_edges(self):
        num_msgs = range(10, 101, 10)
        msgs = []
        latency = []
        for n in num_msgs:
            #for i in range(NUM_SAMPLES):
            msgs.append(n)
            latency.append(self.do_test(n))
        # record latencies
        df = pd.DataFrame({'num_msgs':msgs, 'latency':latency})
        write_csv('nbody', 'num_msgs_latency.csv', df)

class Multithread(unittest.TestCase):
    def test_basic(self):
        pkt_len = 64 # bytes
        msg_len = pkt_len - len(lnic_req()) # bytes
        payload = Raw('\x00'*msg_len)
        sendp(lnic_req(1) / payload, iface=TEST_IFACE)
        sendp(lnic_req(0) / payload, iface=TEST_IFACE)
        self.assertTrue(True)

