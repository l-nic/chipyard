
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
import INT_headers as INT
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

NIC_IP = "10.0.0.2"
MY_IP = "10.0.0.1"

DST_CONTEXT = 0
LATENCY_CONTEXT = 0x1234 # use this when we want the HW to insert timestamps into DATA pkts
DEFAULT_CONTEXT = 0x5678

LOG_DIR = '/vagrant/logs'

NUM_SAMPLES = 1

def lnic_pkt(msg_len, pkt_offset, src_context=DEFAULT_CONTEXT, dst_context=DST_CONTEXT, src_ip=MY_IP, tx_msg_id=0):
    return Ether(dst=NIC_MAC, src=MY_MAC) / \
            IP(src=src_ip, dst=NIC_IP) / \
            LNIC(flags='DATA', src_context=src_context, dst_context=dst_context, msg_len=msg_len, pkt_offset=pkt_offset, tx_msg_id=tx_msg_id)

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

    def app_msg(self, dst_context, service_time, pkt_len):
        msg_len = pkt_len - len(Ether()/IP()/LNIC())
        return lnic_pkt(msg_len, 0, src_context=LATENCY_CONTEXT, dst_context=dst_context) / DummyApp.DummyApp(service_time=service_time) / \
               Raw('\x00'*(pkt_len - len(Ether()/IP()/LNIC()/DummyApp.DummyApp())))

    def test_scheduler(self):
        num_c0_msgs = 30
        num_c1_msgs = 30
        service_time = 1600
        inputs = []
        # context 0 msgs
        inputs += [self.app_msg(0, service_time, 80) for i in range(num_c0_msgs)]
        # context 1 msgs
        inputs += [self.app_msg(1, service_time, 80) for i in range(num_c1_msgs)]
        # shuffle pkts
        random.shuffle(inputs)

        # set tx_msg_ids
        tx_msg_id = 0
        for p in inputs:
            p[LNIC].tx_msg_id = tx_msg_id % 128
            tx_msg_id += 1

        receiver = LNICReceiver(TEST_IFACE)
        # start sniffing for responses
        sniffer = AsyncSniffer(iface=TEST_IFACE, lfilter=lambda x: x.haslayer(LNIC) and x[LNIC].flags.DATA and x[LNIC].dst_context == LATENCY_CONTEXT,
                    prn=receiver.process_pkt, count=num_c0_msgs + num_c1_msgs, timeout=200)
        sniffer.start()
        # send in pkts
        sendp(inputs, iface=TEST_IFACE, inter=1.1)
        # wait for all responses
        sniffer.join()
        # check responses
        self.assertEqual(len(sniffer.results), num_c0_msgs + num_c1_msgs)
        time = []
        context = []
        latency = []
        service_time = []
        for p in sniffer.results:
            self.assertTrue(p.haslayer(LNIC))
            l = struct.unpack('!L', str(p)[-4:])[0]
            t = struct.unpack('!L', str(p)[-8:-4])[0]
            s = p[DummyApp.DummyApp].service_time
            self.assertTrue(p[LNIC].src_context in [0, 1])
            time.append(t)
            context.append(p[LNIC].src_context)
            latency.append(l)
            service_time.append(s)
        # record latencies in a DataFrame
        df = pd.DataFrame({'time': pd.Series(time), 'context': pd.Series(context), 'latency': pd.Series(latency), 'service_time': pd.Series(service_time)}, dtype=float)
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
    def read_msg(nodes=[NIC_IP], key=0):
        cr_hdr = ChainRep(flags='FROM_TEST', nodes=nodes[1:], client_ip=MY_IP, op=CHAINREP_OP_READ, seq=0, key=key, value=0)
        return Ether(dst=NIC_MAC, src=MY_MAC) / \
                IP(src=MY_IP, dst=nodes[0][0]) / \
                LNIC(flags='DATA', src_context=DEFAULT_CONTEXT, dst_context=nodes[0][1], msg_len=len(cr_hdr), pkt_offset=0) / cr_hdr
    @staticmethod
    def write_msg(nodes=[NIC_IP], seq=0, key=0, val=0):
        cr_hdr = ChainRep(flags='FROM_TEST', nodes=nodes[1:], client_ip=MY_IP, op=CHAINREP_OP_WRITE, seq=seq, key=key, value=val)
        return Ether(dst=NIC_MAC, src=MY_MAC) / \
                IP(src=MY_IP, dst=nodes[0][0]) / \
                LNIC(flags='DATA', src_context=DEFAULT_CONTEXT, dst_context=nodes[0][1], msg_len=len(cr_hdr), pkt_offset=0) / cr_hdr

    def stop_filter(self, p):
        return p[IP].dst == MY_IP

    def fwd_pkt(self, p):
        self.assertTrue(p.haslayer(ChainRep))
        if p[IP].dst == MY_IP: return
        resp = p.copy()
        resp[ChainRep].flags = 'FROM_TEST'
        sendp([resp], iface=TEST_IFACE)

    def test_write(self):
        receiver = LNICReceiver(TEST_IFACE, prn=self.fwd_pkt)
        sniffer = AsyncSniffer(iface=TEST_IFACE, lfilter=lambda x: x.haslayer(LNIC) and x[LNIC].flags.DATA and not x[ChainRep].flags.FROM_TEST,
                    prn=receiver.process_pkt, stop_filter=self.stop_filter, timeout=100)
        sniffer.start()
        nodes = [(NIC_IP, 0), (NIC_IP, 1), (NIC_IP, 2)]
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
        self.assertEqual(p1[IP].dst, nodes[1][0])
        self.assertEqual(p2[IP].dst, nodes[2][0])
        self.assertEqual(p3[IP].dst, req[ChainRep].client_ip)

    def test_read(self):
        receiver = LNICReceiver(TEST_IFACE, prn=self.fwd_pkt)
        sniffer = AsyncSniffer(iface=TEST_IFACE, lfilter=lambda x: x.haslayer(LNIC) and x[LNIC].flags.DATA and not x[ChainRep].flags.FROM_TEST,
                    prn=receiver.process_pkt, stop_filter=self.stop_filter, timeout=100)
        sniffer.start()
        req = ChainReplication.read_msg(nodes=[(NIC_IP, 2)], key=3)
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
        src_ips = ['10.0.0.{}'.format(i) for i in range(3, 3 + num_hosts)]
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


class INTCollectorTest(unittest.TestCase):
    def setUp(self):
        bind_layers(LNIC, INT.NetworkEvent, dst_context=INT.UPSTREAM_COLLECTOR_PORT)
        bind_layers(LNIC, INT.DoneMsg, dst_context=LATENCY_CONTEXT)

    def INT_report(self, flow_dst_port, tx_msg_id, int_metadata):
        p = Ether(dst=NIC_MAC, src=MY_MAC)/ \
            IP(src=MY_IP, dst=NIC_IP, tos=0x17<<2)/ \
            LNIC(flags='DATA', src_context=LATENCY_CONTEXT, dst_context=0, tx_msg_id=tx_msg_id)/ \
            INT.Padding()/ \
            INT.TelemetryReport_v1(ingressTimestamp=1524138290)/ \
            Ether()/ \
            IP(src="10.1.2.3", dst='10.4.5.6')/ \
            UDP(sport=5000, dport=flow_dst_port)/ \
            INT.INT_v1(length=3 + len(int_metadata), hopMLen=8, ins=(1<<7|1<<6|1<<5|1<<4|1<<3|1<<2|1<<1|1)<<8,
                INTMetadata=int_metadata)/ \
            ('\x00'*8) # padding so the HW can insert a timestamp
        p[LNIC].msg_len = len(p) - len(Ether()/IP()/LNIC())
        print "len(INT report) = {} bytes".format(len(p))
        print "len(int_metadata) = {}".format(len(int_metadata))
        return p

    def INT_opt_report(self, flow_dst_port, tx_msg_id, int_metadata):
        p = Ether(dst=NIC_MAC, src=MY_MAC)/ \
            IP(src=MY_IP, dst=NIC_IP, tos=0x17<<2)/ \
            LNIC(flags='DATA', src_context=LATENCY_CONTEXT, dst_context=0, tx_msg_id=tx_msg_id)/ \
            INT.Padding()/ \
            INT.TelemetryReport_v1(ingressTimestamp=1524138290)/ \
            Ether()/ \
            IP(src="10.1.2.3", dst='10.4.5.6')/ \
            UDP(sport=5000, dport=flow_dst_port)/ \
            INT.INT_opt(length=3 + len(int_metadata)*2, hopMLen=16, ins=(1<<7|1<<6|1<<5|1<<4|1<<3|1<<2|1<<1|1)<<8,
                INTMetadata=int_metadata)/ \
            ('\x00'*8) # padding so the HW can insert a timestamp
        p[LNIC].msg_len = len(p) - len(Ether()/IP()/LNIC())
        return p

    def INT_metadata(self, swid, l1_ingress_port, l1_egress_port, hop_latency, qid, qsize,
                           ingress_time, egress_time, l2_ingress_port, l2_egress_port, tx_utilization):
        return [swid, l1_ingress_port<<16 | l1_egress_port, hop_latency, qid<<24 | qsize,
                ingress_time, egress_time, l2_ingress_port<<16 | l2_egress_port, tx_utilization]

#    def test_send_report(self):
#        swid = 1
#        l1_ingress_port = 2
#        l1_egress_port = 3
#        hop_latency = 400
#        qid = 0
#        qsize = 600
#        ingress_time = 700
#        egress_time = 800
#        l2_ingress_port = 5
#        l2_egress_port = 1000
#        tx_utilization = 1
#        int_metadata = self.INT_metadata(swid, l1_ingress_port, l1_egress_port, hop_latency, qid, qsize,
#                         ingress_time, egress_time, l2_ingress_port, l2_egress_port, tx_utilization)
#        #NOTE: we will use the flow_dst_port to index the flow state for now ...
#        report = self.INT_report(flow_dst_port=0, tx_msg_id=0, int_metadata=int_metadata)
#
#        print "Sending report pkt:"
#        report.show()
#        hexdump(report)
#
#        sendp(report, iface=TEST_IFACE)

    def test_collector(self):
        # NOTE: this param must match the constants defined in the collector src file
        NUM_REPORTS = 2

        NUM_HOPS = 6
        # Create INT reports to send
        l1_ingress_port = 2
        l1_egress_port = 3
        hop_latency = 400
        qid = 0
        qsize = 600
        ingress_time = 700
        egress_time = 800
        l2_ingress_port = 5
        l2_egress_port = 1000
        tx_utilization = 1
        int_metadata = []
        for i in range(NUM_HOPS):
            swid = i
            int_metadata += self.INT_metadata(swid, l1_ingress_port, l1_egress_port, hop_latency, qid, qsize,
                             ingress_time, egress_time, l2_ingress_port, l2_egress_port, tx_utilization)
        #NOTE: we will use the flow_dst_port to index the flow state for now ...
        report = self.INT_report(flow_dst_port=0, tx_msg_id=0, int_metadata=int_metadata)
        inputs = [report.copy() for i in range(NUM_REPORTS)]
        # assign a unique LNIC tx_msg_id to each report so that the NIC can properly reassemble
        for p, i in zip(inputs, range(len(inputs))):
            p[LNIC].tx_msg_id = i % 128

        receiver = LNICReceiver(TEST_IFACE)
        # Create two sniffers, one to listen for NetworkEvents
        # and one to listen for the final done msg
        # start sniffing for responses
        exp_num_events = 2 + 3*NUM_HOPS
        event_sniffer = AsyncSniffer(iface=TEST_IFACE, lfilter=lambda x: x.haslayer(INT.NetworkEvent) and x[LNIC].flags.DATA,
                    prn=receiver.process_pkt, count=exp_num_events, timeout=300)
        done_sniffer = AsyncSniffer(iface=TEST_IFACE, lfilter=lambda x: x.haslayer(INT.DoneMsg) and x[LNIC].flags.DATA,
                    prn=receiver.process_pkt, count=1, timeout=300)
        event_sniffer.start()
        done_sniffer.start()
        # send in pkts
        sendp(inputs, iface=TEST_IFACE)
        # wait for all responses
        event_sniffer.join()
        done_sniffer.join()
#        print "-------- Events: ---------"
#        for p in event_sniffer.results:
#            p.show()
#            print '================================'
#            print '================================'
        total_latency = done_sniffer.results[0].latency / 3.2e9 # seconds
        throughput = NUM_REPORTS/total_latency # postcards/second
        print 'throughput = {} M reports/sec'.format(throughput/1e6)


class INTHHTest(unittest.TestCase):
    def setUp(self):
        bind_layers(LNIC, INT.HH_header, dst_context=LATENCY_CONTEXT)

    def INT_report(self, dst_context, flow_id):
        pkt_len = 1500
        ingress_switch_ip = 0x0a010100
        p = Ether(dst=NIC_MAC, src=MY_MAC)/ \
            IP(src=MY_IP, dst=NIC_IP)/ \
            LNIC(flags='DATA', src_context=LATENCY_CONTEXT, dst_context=dst_context)/ \
            INT.INT_HH_report(src_ip='10.2.2.2', dst_ip='10.3.3.3', src_port=0, dst_port=flow_id, proto=0,
                          pkt_len=pkt_len, ingress_switch_ip=ingress_switch_ip)

        p[LNIC].msg_len = len(p) - len(Ether()/IP()/LNIC())
        return p

    def do_hh_test(self, inputs, num_cores, exp_num_events):
        receiver = LNICReceiver(TEST_IFACE)
        # Create two sniffers, one to listen for NetworkEvents
        # and one to listen for the final done msg
        # start sniffing for responses
        event_sniffer = AsyncSniffer(iface=TEST_IFACE, lfilter=lambda x: x.haslayer(INT.HH_event) and x[LNIC].flags.DATA,
                    prn=receiver.process_pkt, count=exp_num_events, timeout=300) if exp_num_events > 0 else None
        done_sniffer = AsyncSniffer(iface=TEST_IFACE, lfilter=lambda x: x.haslayer(INT.DoneMsg) and x[LNIC].flags.DATA,
                    prn=receiver.process_pkt, count=num_cores, timeout=300)
        if event_sniffer is not None:
            event_sniffer.start()
        done_sniffer.start()
        # send in pkts
        sendp(inputs, iface=TEST_IFACE)
        # wait for all responses
        if event_sniffer is not None:
            event_sniffer.join()
        done_sniffer.join()
        if event_sniffer is not None:
            print "-------- Events: ---------"
            for p in event_sniffer.results:
                p.show()
                print '================================'
                print '================================'
        total_latency = done_sniffer.results[-1].latency / 3.2e9 # seconds
        throughput = len(inputs)/total_latency # postcards/second
        print 'throughput = {} M reports/sec'.format(throughput/1e6)

#    def test_num_cores(self):
#        # NOTE: this param must match the constants defined in the collector src file
#        NUM_CORES = 4
#        NUM_REPORTS_PER_CORE = 50
#
#        # TODO(sibanez): generate reports and compute expected number of HH events that should be generated.
#        inputs = []
#        for i in range(NUM_REPORTS_PER_CORE):
#            for c in range(NUM_CORES):
#                p = self.INT_report(dst_context=c, flow_id=0)
#                p[LNIC].tx_msg_id = (c*NUM_REPORTS_PER_CORE + i) % 128
#                p[INT.INT_HH_report].report_timestamp = i*5000
#                p[INT.INT_HH_report].flow_flags.DATA = True
#                if i == 0:
#                    p[INT.INT_HH_report].flow_flags.START = True
#                inputs.append(p)
#
#        self.do_hh_test(inputs, NUM_CORES, 0)

#    def test_num_flows(self):
#        NUM_FLOWS = 10
#        NUM_REPORTS_PER_FLOW = 3
#        REPEAT_FLOWS = 1
#        # NOTE: NUM_REPORTS_PER_CORE = NUM_FLOWS * NUM_REPORTS_PER_FLOW * REPEAT_FLOWS
#
#        inputs = []
#        for f in range(NUM_FLOWS):
#            for i in range(NUM_REPORTS_PER_FLOW):
#                p = self.INT_report(dst_context=0, flow_id=f)
#                p[LNIC].tx_msg_id = (f*NUM_FLOWS + i) % 128
#                p[INT.INT_HH_report].report_timestamp = i*5000
#                p[INT.INT_HH_report].flow_flags.DATA = True
#                if i == 0:
#                    p[INT.INT_HH_report].flow_flags.START = True
#                if i == NUM_REPORTS_PER_FLOW-1:
#                    p[INT.INT_HH_report].flow_flags.FIN = True
#                inputs.append(p)
#        inputs = inputs * REPEAT_FLOWS
#
#        self.do_hh_test(inputs, 1, 0)

    def test_detection_latency(self):
        NUM_REPORTS = 2

        inputs = []
        for i in range(NUM_REPORTS):
            p = self.INT_report(dst_context=0, flow_id=0)
            p[LNIC].tx_msg_id = i % 128
            p[INT.INT_HH_report].report_timestamp = i*500
            p[INT.INT_HH_report].flow_flags.DATA = True
            if i == 0:
                p[INT.INT_HH_report].flow_flags.START = True
            inputs.append(p)

        self.do_hh_test(inputs, 1, 1)


class INTPathLatencyTest(unittest.TestCase):
    def setUp(self):
        bind_layers(LNIC, INT.HH_header, dst_context=LATENCY_CONTEXT)

    def INT_report(self, dst_context, flow_id, hop_latencies):
        pkt_len = 1500
        p = Ether(dst=NIC_MAC, src=MY_MAC)/ \
            IP(src=MY_IP, dst=NIC_IP)/ \
            LNIC(flags='DATA', src_context=LATENCY_CONTEXT, dst_context=dst_context)/ \
            INT.INT_PathLatency_report(src_ip='10.2.2.2', dst_ip='10.3.3.3', src_port=0, dst_port=flow_id, proto=0,
                          num_hops=len(hop_latencies), hop_latencies=hop_latencies)

        p[LNIC].msg_len = len(p) - len(Ether()/IP()/LNIC())
        return p

    def test_path_latency(self):
        # NOTE: this must match the source file
        NUM_REPORTS_PER_CORE = 50

        NUM_CORES = 1
        NUM_HOPS = 4

        hop_latencies = [100 for i in range(NUM_HOPS)]
        inputs = []
        for i in range(NUM_REPORTS_PER_CORE):
            for c in range(NUM_CORES):
                p = self.INT_report(dst_context=c, flow_id=0, hop_latencies=hop_latencies)
                p[INT.INT_PathLatency_report].flow_flags.DATA = True
                if i == 0:
                    p[INT.INT_PathLatency_report].flow_flags.START = True
                inputs.append(p)

        for p, i in zip(inputs, range(len(inputs))):
            p[LNIC].tx_msg_id = i % 128

        receiver = LNICReceiver(TEST_IFACE)

        done_sniffer = AsyncSniffer(iface=TEST_IFACE, lfilter=lambda x: x.haslayer(INT.DoneMsg) and x[LNIC].flags.DATA,
                    prn=receiver.process_pkt, count=NUM_CORES, timeout=300)
        done_sniffer.start()
        sendp(inputs, iface=TEST_IFACE)
        done_sniffer.join()

        # Send in a request with a really high path latency such that a report is generated

        hop_latencies = [10000 for i in range(NUM_HOPS)]
        p = self.INT_report(dst_context=0, flow_id=0, hop_latencies=hop_latencies)
        p[INT.INT_PathLatency_report].flow_flags.DATA = True

        event_sniffer = AsyncSniffer(iface=TEST_IFACE, lfilter=lambda x: x.haslayer(INT.PathLatencyAnomaly_event) and x[LNIC].flags.DATA,
                    prn=receiver.process_pkt, count=1, timeout=300)
        event_sniffer.start()
        sendp(p, iface=TEST_IFACE)
        event_sniffer.join()

        print "-------- Events: ---------"
        for p in event_sniffer.results:
            p.show()
            print '================================'
            print '================================'
        total_latency = done_sniffer.results[-1].latency / 3.2e9 # seconds
        throughput = len(inputs)/total_latency # postcards/second
        print 'throughput = {} M reports/sec'.format(throughput/1e6)


class PostcardOptTest(unittest.TestCase):
    def setUp(self):
        bind_layers(LNIC, Postcard.RawPostcard, dst_context=0)
        bind_layers(LNIC, Postcard.AggPostcard, dst_context=Postcard.UPSTREAM_COLLECTOR_PORT)
        bind_layers(LNIC, Postcard.DoneMsg, dst_context=LATENCY_CONTEXT)

    def raw_postcard(self, num_hops, pkt_offset, tx_msg_id):
        msg_len = len(Postcard.RawPostcard())*num_hops
        src_context = LATENCY_CONTEXT
        dst_context = 0
        return lnic_pkt(msg_len, pkt_offset, src_context, dst_context, tx_msg_id=tx_msg_id) / \
                 Postcard.RawPostcard(tx_msg_id=tx_msg_id, qtime=1)

    def test_collector(self):
        # NOTE: these params must match the constants defined in the collector src file
        NUM_PKTS = 20
        NUM_HOPS = 3
        # Create raw postcards to send
        inputs = []
        for i in range(NUM_PKTS):
            # All postcards corresponding to the same pkt must use the same tx_msg_id so they can
            # be reassembled by the NIC.
            inputs += [self.raw_postcard(NUM_HOPS, pkt_offset=j, tx_msg_id=i) for j in range(NUM_HOPS)]

        receiver = LNICReceiver(TEST_IFACE)
        # Create two sniffers, one to listen for aggregated postcards
        # and one to listen for the final done msg
        # start sniffing for responses
        postcard_sniffer = AsyncSniffer(iface=TEST_IFACE, lfilter=lambda x: x.haslayer(Postcard.AggPostcard) and x[LNIC].flags.DATA,
                    prn=receiver.process_pkt, count=NUM_PKTS, timeout=200)
        done_sniffer = AsyncSniffer(iface=TEST_IFACE, lfilter=lambda x: x.haslayer(Postcard.DoneMsg) and x[LNIC].flags.DATA,
                    prn=receiver.process_pkt, count=1, timeout=200)
        postcard_sniffer.start()
        done_sniffer.start()
        # send in pkts
        sendp(inputs, iface=TEST_IFACE)
        # wait for all responses
        postcard_sniffer.join()
        done_sniffer.join()
        # check responses
        for p in postcard_sniffer.results:
            self.assertEqual(p[Postcard.AggPostcard].total_qtime, NUM_HOPS)
        total_latency = done_sniffer.results[0].latency / 3.2e9 # seconds
        throughput = (NUM_PKTS * NUM_HOPS)/total_latency # postcards/second
        print 'throughput = {} M postcards/sec'.format(throughput/1e6)


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

