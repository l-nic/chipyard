from scapy.all import *

CHAINREP_PROTO = 0x98

CHAINREP_OP_READ  = 1
CHAINREP_OP_WRITE = 2

class LongIPField(IPField):
    def __init__(self, name, default):
        Field.__init__(self, name, default, "8s")
    def i2m(self, pkt, x):
        if x is None:
            return b'\x00\x00\x00\x00\x00\x00\x00\x00'
        return b'\x00\x00\x00\x00' + inet_aton(plain_str(x))
    def m2i(self, pkt, x):
        return inet_ntoa(x[4:])

class ChainRep(Packet):
    fields_desc=[
       IPField("client_ip", "0.0.0.0"),
       ByteEnumField("op", 1, {1:"READ", 2:"WRITE"}),
       ByteField("seq", 0),
       FieldLenField("node_cnt", None, count_of="nodes", fmt="H"),
       FieldListField("nodes", ["1.2.3.4"], LongIPField("", "0.0.0.0"),
                       count_from = lambda pkt: pkt.node_cnt),
       XLongField("key", 0),
       XLongField("value", 0)
       ]

bind_layers(IP, ChainRep, proto=CHAINREP_PROTO)

if __name__ == '__main__':
    cr_hdr = ChainRep(nodes=['1.1.1.1', '2.2.2.2', '3.3.3.3'], op=1, seq=0, key=3, value=7)
    p = Ether(dst='00:11:22:33:44:55', src='00:11:22:33:44:56') / IP() / cr_hdr
    p.show2()
    print ' '.join(hex(ord(x)) for x in str(p))
    print "len", len(p)
