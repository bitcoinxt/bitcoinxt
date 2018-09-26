from test_framework.cdefs import MIN_TX_SIZE, MAX_TXOUT_PUBKEY_SCRIPT
from test_framework.mininode import CTransaction, FromHex, ToHex, CTxOut
from test_framework.script import OP_RETURN, CScript

import random
from binascii import hexlify, unhexlify

# Append junk outputs until it reaches at least min_size
def bloat_tx(tx, min_size = None):
    if min_size is None:
        min_size = MIN_TX_SIZE

    curr_size = len(tx.serialize())

    while curr_size < min_size:
        # txout.value + txout.pk_script bytes + op_return
        extra_bytes = 8 + 1 + 1
        junk = max(0, min_size - curr_size - extra_bytes)
        junk = min(junk, MAX_TXOUT_PUBKEY_SCRIPT)
        if junk == 0:
            tx.vout.append(CTxOut(0, CScript([OP_RETURN])))
        else:
            tx.vout.append(CTxOut(0, CScript([random.getrandbits(8 * junk), OP_RETURN])))
        curr_size = len(tx.serialize())

    tx.rehash()

# Append junk outputs until it reaches at least min_size
def bloat_raw_tx(rawtx_hex, min_size = None):

    tx = CTransaction()
    FromHex(tx, rawtx_hex)
    bloat_tx(tx, min_size)
    return ToHex(tx)
