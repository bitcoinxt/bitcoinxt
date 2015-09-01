#!/usr/bin/env python2
# Copyright (c) 2014 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

#
# Test -maxmempooltx limit-number-of-transactions-in-mempool
# code
#

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import *
import os
import shutil
import time

class MempoolCoinbaseTest(BitcoinTestFramework):

    def setup_network(self):
        # Three nodes
        args = ["-maxmempooltx=11", "-checkmempool", "-debug=mempool"]
        self.nodes = []
        self.nodes.append(start_node(0, self.options.tmpdir, args))
        self.nodes.append(start_node(1, self.options.tmpdir, args))
        self.nodes.append(start_node(2, self.options.tmpdir, args))
        connect_nodes_bi(self.nodes, 0, 1)
        connect_nodes_bi(self.nodes, 0, 2)
        self.is_network_split = False

    def create_tx(self, from_txid, to_address, amount, node=0):
        inputs = [{ "txid" : from_txid, "vout" : 0}]
        outputs = { to_address : amount }
        rawtx = self.nodes[node].createrawtransaction(inputs, outputs)
        signresult = self.nodes[node].signrawtransaction(rawtx)
        assert_equal(signresult["complete"], True)
        return signresult["hex"]

    def run_test(self):
        node0_address = self.nodes[0].getnewaddress()
        node1_address = self.nodes[1].getnewaddress()

        # 20 transactions
        b = [ self.nodes[0].getblockhash(n) for n in range(1, 21) ]
        coinbase_txids = [ self.nodes[0].getblock(h)['tx'][0] for h in b ]
        spends1_raw = [ self.create_tx(txid, node0_address, 50) for txid in coinbase_txids ]
        spends1_id = [ self.nodes[0].sendrawtransaction(tx) for tx in spends1_raw ]

        # -maxmempooltx doesn't evict transactions submitted via sendrawtransaction:
        assert_equal(len(self.nodes[0].getrawmempool()), 20)

        # ... and doesn't evict sendtoaddress() transactions:
        spends1_id.append(self.nodes[0].sendtoaddress(node0_address, 50))
        assert_equal(len(self.nodes[0].getrawmempool()), 21)

        time.sleep(1) # wait just a bit for node0 to send transactions to node1

        # ... node1's mempool should be limited
        assert(len(self.nodes[1].getrawmempool()) <= 11)

        # have other node create five transactions...
        node1_txids = []
        for i in range(5):
            node1_txids.append(self.nodes[1].sendtoaddress(node1_address, 50))

        # it's mempool should be limited, but should contain those txids:
        node1_txs = set(self.nodes[1].getrawmempool())
        assert(node1_txs.issuperset(node1_txids))
        # The first send-to-self is guaranteed to evict another of node0's transaction.
        # The second (and subsequent) might try (and fail) to evict the first
        # send-to-self, in which case the mempool size can be bigger than -maxmempooltx
        assert(len(self.nodes[1].getrawmempool()) <= 11+4)

        time.sleep(1)

        # node0's mempool should still have all its transactions:
        node0_txs = set(self.nodes[0].getrawmempool())
        assert(node0_txs.issuperset(spends1_id))


        # Have each node mine a block, should empty mempools:
        blocks = []
        blocks.extend(self.nodes[0].generate(1))
        sync_blocks(self.nodes)      
        blocks.extend(self.nodes[1].generate(1))
        sync_blocks(self.nodes)      

        assert_equal(len(self.nodes[0].getrawmempool()), 0)
        assert_equal(len(self.nodes[1].getrawmempool()), 0)

        # Second test:
        #    eviction of long chains of dependent transactions
        parent_ids = [spends1_id[0], node1_txids[0]]
        send_addresses = [node0_address, node1_address]
        chained_ids = [[],[]]
        send_amount = 50
        for i in range(5):
            send_amount = send_amount-0.001 # send with sufficient fee
            for node in range(2):
                raw = self.create_tx(parent_ids[node], send_addresses[node], send_amount, node)
                parent_ids[node] = self.nodes[node].sendrawtransaction(raw)
                chained_ids[node].append(parent_ids[node])

        sync_mempools(self.nodes) # Wait for all ten txns in all mempools
        assert_equal(len(self.nodes[2].getrawmempool()), 10)

        # Have both nodes generate high-priority transactions to exercise chained-transaction-eviction
        # code
        tx_ids = [[],[]]
        for i in range(3):
            for node in range(2):
                tx_ids[node].append(self.nodes[node].sendtoaddress(send_addresses[node], 25))

        # Give a little time for transactions to make their way to node2, it's mempool should
        # remain under limit
        time.sleep(1)
        assert(len(self.nodes[2].getrawmempool()) <= 11)

if __name__ == '__main__':
    MempoolCoinbaseTest().main()
