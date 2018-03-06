#!/usr/bin/env python3
# Copyright (c) 2018 The Bitcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import *
from test_framework.blocktools import *
from test_framework.mininode import *

def build_block_on_tip(node = None, txs = None, prev_height = None, prev_hash = None, prev_mtp = None):
    prev_height = prev_height or node.getblockcount()
    prev_hash = prev_hash or node.getbestblockhash()
    prev_mtp = prev_mtp or node.getblockheader(prev_hash)['mediantime']

    new_height = prev_height + 1
    new_mtp = prev_mtp + 1

    block = create_block(int(prev_hash, 16), create_coinbase(absoluteHeight = new_height), new_mtp)
    block.nVersion = 4

    if txs is not None:
        for tx in txs:
            block.vtx.append(tx)
        block.hashMerkleRoot = block.calc_merkle_root()

    block.solve()
    return { "block" : block, "height" : new_height, "hash" : block.hash, "mtp" : new_mtp }

def assert_tip_is(sha256, xt_node, test_node):
    test_node.sync_with_ping()
    assert_equal(int(xt_node.getbestblockhash(), 16), sha256)

def create_utxos(test_node, xt_node, num_utxos):

    # Generate 100 blocks, so we get enough coinbase depth on first
    blocks = [ build_block_on_tip(xt_node) ]
    for _ in range(100):
        prev = blocks[-1]
        blocks.append(build_block_on_tip(
            prev_height = prev["height"],
            prev_hash = prev["hash"],
            prev_mtp = prev["mtp"]))

    for b in blocks:
        test_node.send_message(msg_block(b["block"]))
    assert_tip_is(blocks[-1]["block"].sha256, xt_node, test_node)

    utxos = [ ]
    UTXOS_PER_BLOCK = 100
    pingpong = 0
    while (len(utxos) < num_utxos):
        coinbase = blocks.pop(0)["block"].vtx[0]

        # Create anyone-can-spend utxos
        total_value = coinbase.vout[0].nValue
        out_value = total_value // UTXOS_PER_BLOCK
        tx = CTransaction()
        tx.vin.append(CTxIn(COutPoint(coinbase.sha256, 0), b''))
        for i in range(UTXOS_PER_BLOCK):
            tx.vout.append(CTxOut(out_value, CScript([OP_TRUE])))
        tx.rehash()

        for i in range(UTXOS_PER_BLOCK):
            utxos.append({ "sha256" : tx.sha256, "i" : i, "value" : out_value})

        tip = blocks[-1]
        blocks.append(build_block_on_tip(txs = [tx],
                                         prev_height = tip["height"],
                                         prev_hash = tip["hash"],
                                         prev_mtp = tip["mtp"]))
        new_tip = blocks[-1]["block"]

        # pingpongs are slow, but we can't blast too many blocks at the node at a time
        if pingpong % 100 == 0:
            test_node.send_and_ping(msg_block(new_tip))
        else:
            test_node.send_message(msg_block(new_tip))
        pingpong += 1

    assert_tip_is(blocks[-1]["block"].sha256, xt_node, test_node)
    assert_equal(int(xt_node.getbestblockhash(), 16), blocks[-1]["block"].sha256)
    return utxos

# Creates a tx that spends one input (and has no outputs)
def create_small_tx(utxo):
        tx = CTransaction()
        tx.vin.append(CTxIn(COutPoint(utxo["sha256"], utxo["i"]), b''))
        tx.vout.append(CTxOut(0, CScript([OP_TRUE])))
        tx.rehash()
        return tx

# TestNode: A peer we use to send messages to bitcoind, and store responses.
class TestNode(SingleNodeConnCB):

    def __init__(self):
        SingleNodeConnCB.__init__(self)
        self.last_sendcmpct = None
        self.last_getheaders = None
        self.last_headers = None
        self.last_getblocktxn = None
        self.last_cmpctblock = None
        self.last_blocktxn = None

    def on_sendcmpct(self, conn, message):
        self.last_sendcmpct = message

    def on_getheaders(self, conn, message):
        self.last_getheaders = message

    def on_headers(self, conn, message):
        self.last_headers = message

    def on_getblocktxn(self, conn, message):
        self.last_getblocktxn = message

    def on_cmpctblock(self, conn, message):
        self.last_cmpctblock= message

    def on_blocktxn(self, conn, message):
        self.last_blocktxn = message

    def on_inv(self, conn, message):
        pass

    def handshake(self):
        self.wait_for_verack()

        # Exchange sendcmpct
        got_sendcmpt = wait_until(lambda: self.last_sendcmpct != None)
        assert(got_sendcmpt)

        sendcmpct = msg_sendcmpct()
        sendcmpct.version = 1
        sendcmpct.announce = True
        self.send_and_ping(sendcmpct)

        # Exchange headers (just mirror header request/response)
        got_getheaders = wait_until(lambda: self.last_getheaders != None)
        assert(got_getheaders)
        self.send_message(self.last_getheaders)

        got_headers = wait_until(lambda: self.last_headers != None)
        assert(got_headers)
        self.send_message(self.last_headers)

class HFBumpTest(BitcoinTestFramework):
    def __init__(self):
        super().__init__()
        self.num_nodes = 1

    def run_test(self):
        xt_node = self.nodes[0]

        # generate a block to get out of IBD
        xt_node.generate(1)

        test_node = TestNode()
        test_node.add_connection(NodeConn('127.0.0.1', p2p_port(0), xt_node, test_node))

        NetworkThread().start()
        test_node.handshake()

        transactions = 128000
        self._test_prefilled_limits(xt_node, test_node, transactions)
        self._test_getblocktxn_limts(xt_node, test_node, transactions)

    def setup_network(self, split = False):
        self.extra_args = [['-debug=thin']]
        self.nodes = self.setup_nodes()
        self.is_network_split = split

    def _prepare_block(self, xt_node, test_node, transactions):
        print("Creating UTXOS...")
        utxos = create_utxos(test_node, xt_node, transactions)

        print("Generating transactions...")
        txs = [ create_small_tx(u) for u in utxos ]

        print("Building block with %d transactions..." % len(txs))
        block = build_block_on_tip(node = xt_node, txs = txs)["block"]
        return block

    def _test_prefilled_limits(self, xt_node, test_node, transactions):
        print("Testing prefilled limits")
        block = self._prepare_block(xt_node, test_node, transactions)

        print("Sending compact block...")
        # Prefill coinbase + the last transaction.
        # This checks that PrefilledTransaction::index can handle large offsets.
        prefilled = [0, len(block.vtx) -1]
        comp_block = HeaderAndShortIDs();
        comp_block.initialize_from_block(block)
        test_node.send_and_ping(msg_cmpctblock(comp_block.to_p2p()))

        print("Wait for getblocktxn request...")
        got_getblocktxn = wait_until(lambda: test_node.last_getblocktxn, timeout=30)
        assert(got_getblocktxn)

        absolute_indexes = test_node.last_getblocktxn.block_txn_request.to_absolute()
        expected = [i for i in range(1, len(block.vtx))]
        assert_equal(expected, absolute_indexes)

        print("Sending blocktxn...")
        msg = msg_blocktxn()
        msg.block_transactions.blockhash = block.sha256
        for i in expected:
            msg.block_transactions.transactions.append(block.vtx[i])
        test_node.send_and_ping(msg)
        assert_tip_is(block.sha256, xt_node, test_node)

    def _test_getblocktxn_limts(self, xt_node, test_node, transactions):
        print("Testing getblocktxn limits")
        block = self._prepare_block(xt_node, test_node, transactions)

        test_node.last_cmpctblock = None
        test_node.last_blocktxn = None

        print("Sending block...")
        test_node.send_and_ping(msg_block(block))
        assert_tip_is(block.sha256, xt_node, test_node)

        print("Wait for compact block announcement...")
        got_cmpctblock = wait_until(lambda: test_node.last_cmpctblock != None)

        print("Sending getblocktxn, requesting coinbase + last transaction")
        msg = msg_getblocktxn()
        msg.block_txn_request = BlockTransactionsRequest(block.sha256, [0, len(block.vtx)-2])
        test_node.send_message(msg)

        print("Waiting for blocktxn")
        got_blocktxn = wait_until(lambda: test_node.last_blocktxn != None)

        assert_equal(2, len(test_node.last_blocktxn.block_transactions.transactions))
        coinbase = test_node.last_blocktxn.block_transactions.transactions[0]
        last = test_node.last_blocktxn.block_transactions.transactions[1]
        coinbase.calc_sha256()
        last.calc_sha256()
        assert_equal(block.vtx[0].hash, coinbase.hash)
        assert_equal(block.vtx[-1].hash, last.hash)

if __name__ == '__main__':
    HFBumpTest().main()
