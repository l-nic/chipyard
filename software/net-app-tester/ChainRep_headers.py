from scapy.all import *

CHAINREP_PROTO = 0x98

CHAINREP_OP_READ  = 1
CHAINREP_OP_WRITE = 2

class IPCTXField(IPField):
    def __init__(self, name, default):
        Field.__init__(self, name, default, "8s")
    def i2m(self, pkt, x):
        if x is None:
            return b'\x00\x00\x00\x00\x00\x00\x00\x00'
        assert len(x) == 2
        ip, ctx = x
        return b'\x00\x00' + struct.pack('!H', ctx) + inet_aton(plain_str(ip))
    def m2i(self, pkt, x):
        return (inet_ntoa(x[4:]), struct.unpack('!H', x[2:4])[0])

class ChainRep(Packet):
    fields_desc=[
       FlagsField("flags", 0, 8, ["FROM_TEST", "F2", "F3", "F4", "F5", "F6", "F7", "F8"]),
       ByteEnumField("op", 1, {1:"READ", 2:"WRITE"}),
       ByteField("seq", 0),
       FieldLenField("node_cnt", None, count_of="nodes", fmt="B"),
       IPField("client_ip", "0.0.0.0"),
       FieldListField("nodes", [("1.2.3.4", 0)], IPCTXField("", ("0.0.0.0", 0)),
                       count_from = lambda pkt: pkt.node_cnt),
       XLongField("key", 0),
       XLongField("value", 0)
       ]

bind_layers(IP, ChainRep, proto=CHAINREP_PROTO)

if __name__ == '__main__':
    cr_hdr = ChainRep(nodes=[('1.1.1.1', 0), ('2.2.2.2', 1), ('3.3.3.3', 2)], op=1, seq=0, key=3, value=7)
    p = Ether(dst='00:11:22:33:44:55', src='00:11:22:33:44:56') / IP() / cr_hdr
    p.show2()
    print ' '.join(hex(ord(x)) for x in str(p))
    print "len", len(p)
