#!/usr/bin/env python3
# Copyright (c) 2014-2016 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

# Test mempool limiting together/eviction with the wallet

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import *

class MempoolLimitTest(BitcoinTestFramework):

    def __init__(self):
        super().__init__()
        self.setup_clean_chain = True
        self.num_nodes = 2
        self.txouts = gen_return_txouts()

    def setup_network(self):
        self.nodes = []
        self.nodes.append(start_node(0, self.options.tmpdir, ["-spendzeroconfchange=0", "-debug"]))
        self.nodes.append(start_node(1, self.options.tmpdir, ["-maxmempool=5", "-spendzeroconfchange=0", "-debug"]))
        connect_nodes(self.nodes[0], 1)
        self.is_network_split = False
        self.sync_all()
        self.relayfee = self.nodes[0].getnetworkinfo()['relayfee']

    def run_test(self):
        txids = []
        utxos = create_confirmed_utxos(self.relayfee, self.nodes[0], 130, 102)
        self.sync_all()

        #create a mempool tx that will be evicted by node 1
        us0 = utxos.pop()
        inputs = [{ "txid" : us0["txid"], "vout" : us0["vout"]}]
        outputs = {self.nodes[0].getnewaddress() : us0['amount'] - self.relayfee}
        tx = self.nodes[0].createrawtransaction(inputs, outputs)
        txFS = self.nodes[0].signrawtransaction(tx, None, None, "NONE|FORKID")
        txid0 = self.nodes[0].sendrawtransaction(txFS['hex'])

        #create a mempool tx that will NOT be evicted by node 1 (because submitted there)
        us1 = utxos.pop()
        inputs = [{ "txid" : us1["txid"], "vout" : us1["vout"]}]
        outputs = {self.nodes[0].getnewaddress() : us1['amount'] - self.relayfee}
        tx = self.nodes[0].createrawtransaction(inputs, outputs)
        txFS = self.nodes[0].signrawtransaction(tx, None, None, "NONE|FORKID")
        txid1 = self.nodes[1].sendrawtransaction(txFS['hex'])

        #and now for the spam
        base_fee = self.relayfee*1000
        for i in range (4):
            txids.append([])
            txids[i] = create_lots_of_big_transactions(self.nodes[0], self.txouts, utxos[30*i:30*i+30], 30, base_fee + i*base_fee/10)

        # txid0 should have been evicted by node 1, but txid1 should have been protected
        wait_for(lambda: txid0 not in self.nodes[1].getrawmempool(), what = "tx eviction")
        assert(txid1 in self.nodes[1].getrawmempool())

if __name__ == '__main__':
    MempoolLimitTest().main()
