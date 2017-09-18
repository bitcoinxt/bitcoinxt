#!/usr/bin/env python3
# Copyright (c) 2014-2016 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

# Test mempool limiting together/eviction with the wallet

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import *

class MempoolLimitTest(BitcoinTestFramework):

    def satoshi_round(self, amount):
        return  Decimal(amount).quantize(Decimal('0.00000001'), rounding=ROUND_DOWN)

    def __init__(self):
        # Some pre-processing to create a bunch of OP_RETURN txouts to insert into transactions we create
        # So we have big transactions (and therefore can't fit very many into each block)
        # create one script_pubkey
        script_pubkey = "6a4d0200" #OP_RETURN OP_PUSH2 512 bytes
        for i in range (512):
            script_pubkey = script_pubkey + "01"
        # concatenate 128 txouts of above script_pubkey which we'll insert before the txout for change
        self.txouts = "81"
        for k in range(128):
            # add txout value
            self.txouts = self.txouts + "0000000000000000"
            # add length of script_pubkey
            self.txouts = self.txouts + "fd0402"
            # add script_pubkey
            self.txouts = self.txouts + script_pubkey

    def create_confirmed_utxos(self, count):
        self.nodes[0].generate(int(0.5*count)+102)
        sync_blocks(self.nodes[:2])
        utxos = self.nodes[0].listunspent()
        iterations = count - len(utxos)
        addr1 = self.nodes[0].getnewaddress()
        addr2 = self.nodes[0].getnewaddress()
        if iterations <= 0:
            return utxos
        for i in range(iterations):
            t = utxos.pop()
            fee = self.relayfee
            inputs = []
            inputs.append({ "txid" : t["txid"], "vout" : t["vout"]})
            outputs = {}
            send_value = t['amount'] - fee
            outputs[addr1] = self.satoshi_round(send_value/2)
            outputs[addr2] = self.satoshi_round(send_value/2)
            raw_tx = self.nodes[0].createrawtransaction(inputs, outputs)
            signed_tx = self.nodes[0].signrawtransaction(raw_tx, None, None, "NONE|FORKID")["hex"]
            txid = self.nodes[0].sendrawtransaction(signed_tx)

        while (self.nodes[0].getmempoolinfo()['size'] > 0):
            self.nodes[0].generate(1)

        sync_blocks(self.nodes[:2])

        utxos = self.nodes[0].listunspent()
        assert(len(utxos) >= count)
        return utxos

    def create_lots_of_big_transactions(self, utxos, fee):
        addr = self.nodes[0].getnewaddress()
        txids = []
        for i in range(len(utxos)):
            t = utxos.pop()
            inputs = []
            inputs.append({ "txid" : t["txid"], "vout" : t["vout"]})
            outputs = {}
            send_value = t['amount'] - fee
            outputs[addr] = self.satoshi_round(send_value)
            rawtx = self.nodes[0].createrawtransaction(inputs, outputs)
            newtx = rawtx[0:92]
            newtx = newtx + self.txouts
            newtx = newtx + rawtx[94:]
            signresult = self.nodes[0].signrawtransaction(newtx, None, None, "NONE|FORKID")
            txid = self.nodes[0].sendrawtransaction(signresult["hex"], True)
            txids.append(txid)
        return txids

    def setup_network(self):
        self.nodes = []
        self.nodes.append(start_node(0, self.options.tmpdir, ["-spendzeroconfchange=0", "-debug"]))
        self.nodes.append(start_node(1, self.options.tmpdir, ["-maxmempool=5", "-spendzeroconfchange=0", "-debug"]))
        connect_nodes(self.nodes[0], 1)
        self.is_network_split = False
        self.sync_all()
        self.relayfee = self.nodes[0].getnetworkinfo()['relayfee']

    def setup_chain(self):
        print("Initializing test directory "+self.options.tmpdir)
        initialize_chain_clean(self.options.tmpdir, 2)

    def run_test(self):
        txids = []
        utxos = self.create_confirmed_utxos(130)

        #create a mempool tx that will be evicted by node 1
        us0 = utxos.pop()
        inputs = [{ "txid" : us0["txid"], "vout" : us0["vout"]}]
        outputs = {self.nodes[0].getnewaddress() : us0['amount'] - self.relayfee}
        tx = self.nodes[0].createrawtransaction(inputs, outputs)
        txFS = self.nodes[0].signrawtransaction(tx, None, None, "NONE|FORKID")
        txid0 = self.nodes[0].sendrawtransaction(txFS['hex'])
        self.nodes[0].lockunspent(True, [us0])

        #create a mempool tx that will NOT be evicted by node 1 (because submitted there)
        us1 = utxos.pop()
        inputs = [{ "txid" : us1["txid"], "vout" : us1["vout"]}]
        outputs = {self.nodes[0].getnewaddress() : us1['amount'] - self.relayfee}
        tx = self.nodes[0].createrawtransaction(inputs, outputs)
        txFS = self.nodes[0].signrawtransaction(tx, None, None, "NONE|FORKID")
        txid1 = self.nodes[1].sendrawtransaction(txFS['hex'])
        self.nodes[0].lockunspent(True, [us1])

        sync_mempools(self.nodes[:2])

        #and now for the spam
        base_fee = self.relayfee*1000
        for i in range (4):
            txids.append([])
            txids[i] = self.create_lots_of_big_transactions(utxos[30*i:30*i+30], (i+1)*base_fee)

        time.sleep(3)
        # txid0 should have been evicted by node 1, but txid1 should have been protected
        assert(txid0 not in self.nodes[1].getrawmempool())
        assert(txid1 in self.nodes[1].getrawmempool())

if __name__ == '__main__':
    MempoolLimitTest().main()
