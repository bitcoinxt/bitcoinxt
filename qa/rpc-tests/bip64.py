#!/usr/bin/env python3
#
# Distributed under the MIT/X11 software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
#

import binascii

from test_framework.mininode import SingleNodeConnCB, NodeConn, NetworkThread, \
    wait_until, FromHex, CTransaction, COutPoint, COIN
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import p2p_port, assert_equal, create_confirmed_utxos
from test_framework.bip64 import msg_getutxos
import random

class TestNode(SingleNodeConnCB):
    def __init__(self):
        SingleNodeConnCB.__init__(self)
        self.last_getheaders = None
        self.last_headers = None
        self.utxos = None

    def on_getheaders(self, conn, message):
        self.last_getheaders = message

    def on_headers(self, conn, message):
        self.last_headers = message

    def on_utxos(self, conn, message):
        self.utxos = message

    def handshake(self):
        self.wait_for_verack()

        # Exchange headers (just mirror header request/response)
        got_getheaders = wait_until(lambda: self.last_getheaders != None)
        assert(got_getheaders)
        self.send_message(self.last_getheaders)

        got_headers = wait_until(lambda: self.last_headers != None)
        assert(got_headers)
        self.send_message(self.last_headers)


class BIP64_test(BitcoinTestFramework):

    def __init__(self):
        super().__init__()
        self.num_nodes = 1

    def wait_for_utxo(self):
        got_utxos = wait_until(lambda: self.test_node.utxos != None)
        assert(got_utxos)
        assert_equal(self.test_node.utxos.hash, int(self.xt_node.getbestblockhash(), 16))
        assert_equal(self.test_node.utxos.height, self.xt_node.getblockcount())

    def get_utxos(self, outpoints, checkmempool):
        self.test_node.utxos = None
        self.test_node.send_message(msg_getutxos(checkmempool, outpoints))
        self.wait_for_utxo()

    def run_test(self):
        self.xt_node = self.nodes[0]

        # generate a block to get out of IBD
        self.xt_node.generate(1)

        self.test_node = TestNode()
        self.test_node.add_connection(NodeConn('127.0.0.1', p2p_port(0), self.xt_node, self.test_node))

        NetworkThread().start()
        self.test_node.handshake()

        self.test_basics(self.xt_node, self.test_node)
        self.test_garbage(self.test_node)
        self.test_big_bitmap(self.xt_node, self.test_node, 8 * 2)
        self.test_big_bitmap(self.xt_node, self.test_node, 8 * 128)

    # Requests utxos that never existed
    def test_garbage(self, test_node):
        self.log.info("bip64: test requesting garbage")
        import hashlib
        m = hashlib.sha256()
        outpoints = [ ]
        for _ in range(8):
            m.update(b"garbage")
            outpoints.append(COutPoint(int(m.hexdigest(), 16), random.randrange(1000)))
        self.get_utxos(outpoints, checkmempool = True)
        assert_equal(int('00000000', 2), test_node.utxos.bitmap[0])
        assert_equal(0, len(test_node.utxos.result))

    def test_basics(self, xt_node, test_node):
        self.log.info("bip64: basic checks")
        txid = xt_node.sendtoaddress(xt_node.getnewaddress(), 0.1)
        tx = FromHex(CTransaction(), xt_node.getrawtransaction(txid))
        tx.rehash()

        new_outpoint = COutPoint(tx.sha256, 0)
        prev_outpoint = tx.vin[0].prevout

        # Test that the utxo doesn't exist in the chain.
        self.get_utxos([new_outpoint], checkmempool = False)
        assert_equal(test_node.utxos.bitmap[0], int('0', 2))
        assert_equal(len(test_node.utxos.result), 0)

        # It does exist in the mempool.
        self.get_utxos([new_outpoint], checkmempool = True)
        assert_equal(test_node.utxos.bitmap[0], int('1', 2))
        assert_equal(len(test_node.utxos.result), 1)
        magic_inmempool_height = 2147483647
        assert_equal(test_node.utxos.result[0].height, magic_inmempool_height)

        # The prevout exists in the chain
        self.get_utxos([prev_outpoint], checkmempool = False)
        assert_equal(test_node.utxos.bitmap[0], int('1', 2))

        # The prevout is spent in the mempool.
        self.get_utxos([prev_outpoint], checkmempool = True)
        assert_equal(test_node.utxos.bitmap[0], int('0', 2))

        # Mine tx creating new_outpoint, now it should exist in the chain.
        xt_node.generate(1)
        self.get_utxos([new_outpoint], checkmempool = False)
        assert_equal(test_node.utxos.bitmap[0], int('1', 2))
        assert_equal(test_node.utxos.result[0].height, xt_node.getblockcount())

        # .. same result when including mempool
        self.get_utxos([new_outpoint], checkmempool = True)
        assert_equal(test_node.utxos.bitmap[0], int('1', 2))

        # Check that we can fetch multiple outpoints
        self.get_utxos([new_outpoint, prev_outpoint], checkmempool = False)
        assert_equal(test_node.utxos.bitmap[0], int('01', 2))
        self.get_utxos([prev_outpoint, new_outpoint], checkmempool = False)
        assert_equal(test_node.utxos.bitmap[0], int('10', 2))
        self.get_utxos([new_outpoint, prev_outpoint, new_outpoint], checkmempool = False)
        assert_equal(test_node.utxos.bitmap[0], int('101', 2))

    def test_big_bitmap(self, xt_node, test_node, num_utxos):
        assert(num_utxos % 8 == 0)
        bitmap_bytes = int(num_utxos / 8)
        num_spent = int(num_utxos / 2)

        self.log.info("bip64: creating a getutxos request for %d outpoints (%d bitmap bytes)",
            num_utxos, bitmap_bytes)

        create_confirmed_utxos(
                xt_node.getnetworkinfo()["relayfee"],
                xt_node, num_spent)

        spent = [ ]
        unspent = [ ]
        unspent_values = [ ]

        # spend utxos
        for _ in range(0, num_spent):
            amount = round(random.random() + 0.01, 2)
            txid = xt_node.sendtoaddress(xt_node.getnewaddress(), amount)
            tx = FromHex(CTransaction(), xt_node.getrawtransaction(txid))
            spent.append(tx.vin[0].prevout)

        # find equal amount of unspent
        utxos = create_confirmed_utxos(
                xt_node.getnetworkinfo()["relayfee"],
                xt_node, num_spent)

        assert(len(utxos) >= num_spent)
        for _ in range(0, num_spent):
            u = utxos.pop()
            unspent.append(COutPoint(int(u["txid"], 16), u["vout"]))
            unspent_values.append(u["amount"] * COIN)

        assert(len(spent) + len(unspent) == num_utxos)

        while (xt_node.getmempoolinfo()['size'] > 0):
            xt_node.generate(1)

        outpoints = [ ]
        for b in range(0, bitmap_bytes):
            o = b * 4 # offset: 4 utxos per byte from each of spent, unspent
            outpoints.extend([
                    unspent[o], unspent[o + 1], unspent[o + 2], unspent[o + 3],
                    spent[o], spent[o + 1], spent[o + 2], spent[o + 3] ])

        self.get_utxos(outpoints, False)

        assert_equal(len(test_node.utxos.bitmap), bitmap_bytes)
        assert_equal(len(test_node.utxos.result), len(unspent))
        # check that returned results are in the same order as requested
        for i in range(0, len(unspent_values)):
            assert_equal(unspent_values[i], test_node.utxos.result[i].out.nValue)
        for b in range(0, bitmap_bytes):
            assert_equal(test_node.utxos.bitmap[b], int('00001111', 2))


    def setup_network(self, split = False):
        self.extra_args = [['-debug']]
        self.nodes = self.setup_nodes()
        self.is_network_split = split

if __name__ == '__main__':
    BIP64_test().main()
