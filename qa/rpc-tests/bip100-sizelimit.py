#!/usr/bin/env python2
# Copyright (c) 2017 The Bitcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

#
# Test mining and broadcast of larger-than-1MB-blocks
#
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import *

from decimal import Decimal

CACHE_DIR = "cache_bigblock"

class BigBlockTest(BitcoinTestFramework):

    def setup_chain(self):
        print("Initializing test directory "+self.options.tmpdir)

        if not os.path.isdir(os.path.join(CACHE_DIR, "node0")):
            print("Creating initial chain. This will be cached for future runs.")

            for i in range(4):
                initialize_datadir(CACHE_DIR, i) # Overwrite port/rpcport in bitcoin.conf

            # Node 0 creates 8MB blocks that vote for increase to 8MB
            # Node 1 creates empty blocks that vote for 8MB
            # Node 2 creates empty blocks that vote for 2MB
            # Node 3 creates empty blocks that do not vote for increase
            self.nodes = []
            # Use node0 to mine blocks for input splitting
            self.nodes.append(start_node(0, CACHE_DIR, ["-blockmaxsize=8000000", "-maxblocksizevote=8"]))
            self.nodes.append(start_node(1, CACHE_DIR, ["-blockmaxsize=1000", "-maxblocksizevote=8"]))
            self.nodes.append(start_node(2, CACHE_DIR, ["-blockmaxsize=1000", "-maxblocksizevote=2"]))
            self.nodes.append(start_node(3, CACHE_DIR, ["-blockmaxsize=1000", "-maxblocksizevote=1"]))

            connect_nodes_bi(self.nodes, 0, 1)
            connect_nodes_bi(self.nodes, 1, 2)
            connect_nodes_bi(self.nodes, 2, 3)
            connect_nodes_bi(self.nodes, 3, 0)

            self.is_network_split = False

            # Create a 2012-block chain in a 75% ratio for increase (genesis block votes for 1MB)
            # Make sure they are not already sorted correctly
            blocks = []
            blocks.append(self.nodes[1].generate(503))
            assert(self.sync_blocks(self.nodes[1:3]))
            blocks.append(self.nodes[2].generate(503))
            assert(self.sync_blocks(self.nodes[2:4]))
            blocks.append(self.nodes[3].generate(502)) # <--- genesis is 503rd vote for 1MB
            assert(self.sync_blocks(self.nodes[1:4]))
            blocks.append(self.nodes[1].generate(503))
            assert(self.sync_blocks(self.nodes))

            # Generate 1200 addresses
            addresses = [ self.nodes[3].getnewaddress() for i in range(0,1200) ]

            amount = Decimal("0.00125")

            send_to = { }
            for address in addresses:
                send_to[address] = amount

            tx_file = open(os.path.join(CACHE_DIR, "txdata"), "w")

            # Create four megabytes worth of transactions ready to be
            # mined:
            print("Creating 100 40K transactions (4MB)")
            for node in range(1,3):
                for i in range(0,50):
                    txid = self.nodes[node].sendmany("", send_to, 1)
                    txdata = self.nodes[node].getrawtransaction(txid)
                    tx_file.write(txdata+"\n")
            tx_file.close()

            stop_nodes(self.nodes)
            wait_bitcoinds()
            self.nodes = []
            for i in range(4):
                os.remove(log_filename(CACHE_DIR, i, "debug.log"))
                os.remove(log_filename(CACHE_DIR, i, "db.log"))
                os.remove(log_filename(CACHE_DIR, i, "peers.dat"))
                os.remove(log_filename(CACHE_DIR, i, "fee_estimates.dat"))

        for i in range(4):
            from_dir = os.path.join(CACHE_DIR, "node"+str(i))
            to_dir = os.path.join(self.options.tmpdir,  "node"+str(i))
            shutil.copytree(from_dir, to_dir)
            initialize_datadir(self.options.tmpdir, i) # Overwrite port/rpcport in bitcoin.conf

    def sync_blocks(self, rpc_connections, wait=1, max_wait=30):
        """
        Wait until everybody has the same block count
        """
        for i in range(0,max_wait):
            if i > 0: time.sleep(wait)
            counts = [ x.getblockcount() for x in rpc_connections ]
            if counts == [ counts[0] ]*len(counts):
                return True
        return False

    def setup_network(self):
        self.nodes = []

        self.nodes.append(start_node(0, self.options.tmpdir, ["-blockmaxsize=8000000", "-maxblocksizevote=8"]))
        self.nodes.append(start_node(1, self.options.tmpdir, ["-blockmaxsize=1000", "-maxblocksizevote=8"]))
        self.nodes.append(start_node(2, self.options.tmpdir, ["-blockmaxsize=1000", "-maxblocksizevote=2"]))
        self.nodes.append(start_node(3, self.options.tmpdir, ["-blockmaxsize=1000", "-maxblocksizevote=1"]))
        connect_nodes_bi(self.nodes, 0, 1)
        connect_nodes_bi(self.nodes, 1, 2)
        connect_nodes_bi(self.nodes, 2, 3)
        connect_nodes_bi(self.nodes, 3, 0)

        # Populate node0's mempool with cached pre-created transactions:
        with open(os.path.join(CACHE_DIR, "txdata"), "r") as f:
            for line in f:
                self.nodes[0].sendrawtransaction(line.rstrip())

    def copy_mempool(self, from_node, to_node):
        txids = from_node.getrawmempool()
        for txid in txids:
            txdata = from_node.getrawtransaction(txid)
            to_node.sendrawtransaction(txdata)

    def TestMineBig(self, expect_big):
        # Test if node0 will mine a block bigger than legacy MAX_BLOCK_SIZE
        b1hash = self.nodes[0].generate(1)[0]
        b1 = self.nodes[0].getblock(b1hash, True)
        assert(self.sync_blocks(self.nodes))

        if expect_big:
            assert(b1['size'] > 1000*1000)

            # Have node1 mine on top of the block,
            # to make sure it goes along with the fork
            b2hash = self.nodes[1].generate(1)[0]
            b2 = self.nodes[1].getblock(b2hash, True)
            assert(b2['previousblockhash'] == b1hash)
            assert(self.sync_blocks(self.nodes))

        else:
            assert(b1['size'] < 1000*1000)

        # Reset chain to before b1hash:
        for node in self.nodes:
            node.invalidateblock(b1hash)
        assert(self.sync_blocks(self.nodes))


    def run_test(self):
        # nodes 0 and 1 have mature 50-BTC coinbase transactions.
        # Spend them with 50 transactions, each that has
        # 1,200 outputs (so they're about 41K big).

        print("Testing consensus blocksize increase conditions")

        assert_equal(self.nodes[0].getblockcount(), 2011) # This is a 0-based height

        # Current nMaxBlockSize is still 1MB
        assert_equal(self.nodes[0].getblocktemplate()["sizelimit"], 1000000)
        self.TestMineBig(False)

        # Create a situation where the 1512th-highest vote is for 2MB
        self.nodes[3].generate(1)
        assert(self.sync_blocks(self.nodes[1:4]))
        ahash = self.nodes[1].generate(3)[2]
        assert_equal(self.nodes[1].getblocktemplate()["sizelimit"], int(1000000 * 1.05))
        assert(self.sync_blocks(self.nodes[0:2]))
        self.TestMineBig(True)

        # Shutdown then restart node[0], it should produce a big block.
        stop_node(self.nodes[0], 0)
        self.nodes[0] = start_node(0, self.options.tmpdir, ["-blockmaxsize=8000000", "-maxblocksizevote=8"])
        self.copy_mempool(self.nodes[1], self.nodes[0])
        connect_nodes_bi(self.nodes, 0, 1)
        connect_nodes_bi(self.nodes, 0, 3)
        assert_equal(self.nodes[0].getblocktemplate()["sizelimit"], int(1000000 * 1.05))
        self.TestMineBig(True)

        # Test re-orgs past the sizechange block
        stop_node(self.nodes[0], 0)
        self.nodes[3].invalidateblock(ahash)
        assert_equal(self.nodes[3].getblocktemplate()["sizelimit"], 1000000)
        self.nodes[3].generate(2)
        assert_equal(self.nodes[3].getblocktemplate()["sizelimit"], 1000000)
        assert(self.sync_blocks(self.nodes[1:]))

        # Restart node0, it should re-org onto longer chain,
        # and refuse to mine a big block:
        self.nodes[0] = start_node(0, self.options.tmpdir, ["-blockmaxsize=8000000", "-maxblocksizevote=8"])
        self.copy_mempool(self.nodes[1], self.nodes[0])
        connect_nodes_bi(self.nodes, 0, 1)
        connect_nodes_bi(self.nodes, 0, 3)
        assert(self.sync_blocks(self.nodes))
        assert_equal(self.nodes[0].getblocktemplate()["sizelimit"], 1000000)
        self.TestMineBig(False)

        # Mine 4 blocks voting for 2MB. Bigger block NOT ok, we are in the next voting period
        self.nodes[2].generate(4)
        assert_equal(self.nodes[2].getblocktemplate()["sizelimit"], 1000000)
        assert(self.sync_blocks(self.nodes[0:3]))
        self.TestMineBig(False)


        print("Cached test chain and transactions left in %s"%(CACHE_DIR))

if __name__ == '__main__':
    BigBlockTest().main()
