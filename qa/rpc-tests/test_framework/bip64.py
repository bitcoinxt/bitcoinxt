import struct
import copy
from test_framework.mininode import ser_vector, deser_vector, COutPoint, deser_uint256, deser_compact_size, CTxOut

class msg_getutxos(object):
    command = b"getutxos"

    def __init__(self, checkmempool = False, outpoints = []):
        self.checkmempool = checkmempool
        self.outpoints = outpoints

    def deserialize(self, f):
        self.checkmempool = struct.unpack("<?", f.read(1))[0]
        self.outpoints = deser_vector(f, COutPoint)

    def serialize(self):
        r = b""
        r += struct.pack("<?", self.checkmempool)
        r += ser_vector(self.outpoints)
        return r

    def __repr__(self):
        return "msg_getutxos(checkmempool=%s, outpoints=%s)" % (self.checkmempool, repr(self.outpoints))

class BIP64Coin(object):
    def __init__(self, bip64coin = None):
        if bip64coin is None:
            self.txversion = 0,
            self.height = 0,
            self.out = None
        else:
            self.txversion = bip64coin.txversion
            self.height = bip64.height
            self.out = copy.deepcopy(bip64coin.out)

    def deserialize(self, f):
        self.txversion = struct.unpack("<I", f.read(4))[0]
        self.height = struct.unpack("<I", f.read(4))[0]
        self.out = CTxOut()
        self.out.deserialize(f)

    def serialize(self):
        r = b""
        r += struct.pack("<I", self.txversion)
        r += struct.pack("<I", self.height)
        r += self.out.serialize()
        return r

    def __repr__(self):
        return "BIP64Coin(txversion=%i height=%i out=%s)" % (self.txversion, self.height, repr(self.out))

def deser_byte_vector(f):
    nit = deser_compact_size(f)
    r = []
    for i in range(nit):
        t = struct.unpack("<B", f.read(1))[0]
        r.append(t)
    return r

def ser_byte_vector(l):
    r = ser_compact_size(len(l))
    for i in l:
        r += struct.pack("B", i)
    return r

class msg_utxos(object):
    command = b"utxos"
    def __init__(self, height = 0, hash = 0, bitmap = [], result = []):
        self.height = height
        self.hash = hash
        self.bitmap = bitmap
        self.result = result

    def deserialize(self, f):
        self.height = struct.unpack("<I", f.read(4))[0]
        self.hash = deser_uint256(f)
        self.bitmap = deser_byte_vector(f)
        self.result = deser_vector(f, BIP64Coin)

    def serialize(self):
        r = b""
        r += self.pack("<I", self.height)
        r += ser_uint256(self.hash)
        r += ser_byte_vector(self.bitmap)
        r += ser_vector(BIP64Coin, self.result)
        return r

    def __repr__(self):
        return "msg_utxos(height=%i hash=%064x bitmap=%s result=%s)" % (self.height, self.hash, repr(self.bitmap), repr(self.result))
