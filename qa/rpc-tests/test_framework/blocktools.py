#!/usr/bin/env python3
# blocktools.py - utilities for manipulating blocks and transactions
#
# Distributed under the MIT/X11 software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

from .mininode import *
from .script import CScript, OP_TRUE, OP_CHECKSIG
from test_framework.txtools import bloat_tx

# Create a block (with regtest difficulty)
def create_block(hashprev, coinbase, nTime=None):
    block = CBlock()
    if nTime is None:
        import time
        block.nTime = int(time.time()+600)
    else:
        block.nTime = nTime
    block.hashPrevBlock = hashprev
    block.nBits = 0x207fffff # Will break after a difficulty adjustment...
    block.vtx.append(coinbase)
    block.hashMerkleRoot = block.calc_merkle_root()
    block.calc_sha256()
    return block

def serialize_script_num(value):
    r = bytearray(0)
    if value == 0:
        return r
    neg = value < 0
    absvalue = -value if neg else value
    while (absvalue):
        r.append(int(absvalue & 0xff))
        absvalue >>= 8
    if r[-1] & 0x80:
        r.append(0x80 if neg else 0)
    elif neg:
        r[-1] |= 0x80
    return r

counter=1
# Create a coinbase transaction, assuming no miner fees.
# If pubkey is passed in, the coinbase output will be a P2PK output;
# otherwise an anyone-can-spend output.
def create_coinbase(heightAdjust = 0, absoluteHeight = None, pubkey = None):
    global counter
    height = absoluteHeight if absoluteHeight is not None else counter+heightAdjust
    coinbase = CTransaction()
    coinbase.vin.append(CTxIn(COutPoint(0, 0xffffffff),
                ser_string(serialize_script_num(height)), 0xffffffff))
    counter += 1
    coinbaseoutput = CTxOut()
    coinbaseoutput.nValue = 50 * COIN
    halvings = int(height/150) # regtest
    coinbaseoutput.nValue >>= halvings
    if (pubkey != None):
        coinbaseoutput.scriptPubKey = CScript([pubkey, OP_CHECKSIG])
    else:
        coinbaseoutput.scriptPubKey = CScript([OP_TRUE])
    coinbase.vout = [ coinbaseoutput ]

    # make sure tx is at least MIN_TX_SIZE
    bloat_tx(coinbase)

    coinbase.calc_sha256()
    return coinbase

# Create a transaction.
# If the scriptPubKey is not specified, make it anyone-can-spend.
def create_transaction(prevtx, n, sig, value, scriptPubKey=CScript()):
    tx = CTransaction()
    assert(n < len(prevtx.vout))
    tx.vin.append(CTxIn(COutPoint(prevtx.sha256, n), sig, 0xffffffff))
    tx.vout.append(CTxOut(value, scriptPubKey))
    tx.calc_sha256()
    # make sure tx is at least MIN_TX_SIZE
    bloat_tx(tx)
    return tx

def get_legacy_sigopcount_block(block, fAccurate=True):
    count = 0
    for tx in block.vtx:
        count += get_legacy_sigopcount_tx(tx, fAccurate)
    return count

def get_legacy_sigopcount_tx(tx, fAccurate=True):
    count = 0
    for i in tx.vout:
        count += i.scriptPubKey.GetSigOpCount(fAccurate)
    for j in tx.vin:
        # scriptSig might be of type bytes, so convert to CScript for the moment
        count += CScript(j.scriptSig).GetSigOpCount(fAccurate)
    return count

def ltor_sort_block(block):
    if len(block.vtx) <= 2:
        return

    for tx in block.vtx:
        tx.calc_sha256()

    block.vtx = [block.vtx[0]] + sorted(block.vtx[1:], key=lambda tx: tx.hash)

# Sort by parent-first
def ttor_sort_transactions(txs):
    sorted_txs = []
    index = dict()
    added = set()

    for t in txs:
        t.calc_sha256()
        index[t.sha256] = t

    queue = txs

    def add_or_queue(tx):
        if tx.sha256 in added:
            return
        for i in tx.vin:
            if i.prevout.hash in added:
                continue
            if not i.prevout.hash in index:
                continue

            # child of another tx in list,
            # add back to queue, but add parent in front of it
            queue.append(tx)
            queue.append(index[i.prevout.hash])
            return

        sorted_txs.append(tx)
        added.add(tx.sha256)

    while len(queue):
        add_or_queue(queue.pop())

    return sorted_txs
