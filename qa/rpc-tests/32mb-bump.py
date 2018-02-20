#!/usr/bin/env python3
# Copyright (c) 2018 The Bitcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import *
from test_framework.mininode import ONE_MEGABYTE

"""
This tests that the nodes can handle a max block size limit bump to 32MB at a specified MTP time.
"""

HF_MTP_TIME = 1526400000
MB = ONE_MEGABYTE

# Limit of previous block mined
def get_sizelimit(node):
    return node.getmininginfo()['sizelimit']

def assert_chaintip_within(node, lower, upper):
        block = node.getblock(node.getbestblockhash(), True)
        assert(block['size'] > int(lower) and block['size'] < int(upper))

"""
Mine a block close to target_size, first mining blocks to create utxos if needed.
"""
def mine_block(node, target_bytes, utxos):
    # we will make create_lots_of_big_transactions create a bunch of ~67.5k tx
    import math
    required_txs = math.floor(target_bytes / 67500)

    # if needed, create enough utxos to be able to mine target_size
    fee = 100 * node.getnetworkinfo()["relayfee"]
    if len(utxos) < required_txs:
        utxos.clear()
        utxos.extend(create_confirmed_utxos(fee, node, required_txs))

    create_lots_of_big_transactions(node, gen_return_txouts(), utxos, required_txs, fee)
    node.generate(1)

class HFBumpTest(BitcoinTestFramework):

    def __init__(self):
        super().__init__()
        self.mocktime = 0

    def generate_one(self):
        assert(self.mocktime > 0)
        self.mocktime = self.mocktime + 600
        set_node_times(self.nodes, self.mocktime)
        self.nodes[0].generate(1)

    def run_test(self):
        self._test_initial_bump()
        self._test_mine_big_block()

    def _test_initial_bump(self):
        print("Test that max block size limits goes to 32MB at specified time")
        node = self.nodes[0]
        assert_equal(8 * MB, get_sizelimit(node))

        # time to support 32MB blocks
        self.mocktime = HF_MTP_TIME
        set_node_times(self.nodes, self.mocktime)

        # local time has passed fork point, but mtp hasn't.
        assert_equal(8 * MB, get_sizelimit(node))

        for _ in range(0, 6):
            self.generate_one()

        # mtp has passed fork point, this block isn't first fork block, but next will be
        assert(node.getblockheader(node.getbestblockhash())['mediantime'] > HF_MTP_TIME)
        assert_equal(8 * MB, get_sizelimit(node))

        # fork block
        self.generate_one()
        assert_equal(32 * MB, get_sizelimit(node))

        sync_blocks(self.nodes)
        print("OK!")

    def _test_mine_big_block(self):
        print("Test that we can mine 32MB blocks (and not more)")
        utxo_cache = []
        node = self.nodes[0]
        fee = 100  * node.getnetworkinfo()["relayfee"]

        # Mine a block close to 32MB
        target = 32 * MB
        mine_block(node, target_bytes = target, utxos = utxo_cache)
        assert_chaintip_within(node, target - 0.1*MB, target)

        # See that we don't mine larger than 32MB
        too_big_target = 42 * MB
        mine_block(node, target_bytes = too_big_target, utxos = utxo_cache)
        assert_chaintip_within(node, target - 0.1*MB, target)

        # We were limited by max block size, so there should be transactions left in the mempol.
        assert(node.getmempoolinfo()['bytes'] > too_big_target - target - MB);

        # Clear the mempool
        while node.getmempoolinfo()['bytes']:
            node.generate(1)

        # All nodes should agree on best block
        sync_blocks(self.nodes)
        print("OK!")

if __name__ == '__main__':
    HFBumpTest().main()
