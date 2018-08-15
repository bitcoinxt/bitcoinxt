#!/usr/bin/env python3
#
# Distributed under the MIT/X11 software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
#

import binascii

from test_framework.mininode import *
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import *
from numpy.ma.testutils import assert_equal

'''
spv.py

Act as an SPV client to test SPV server functionality
This is not yet a complete suite.  It was added to test inclusion of ancestors
in connection filters.
'''

class BaseNode(NodeConnCB):
    def __init__(self):
        NodeConnCB.__init__(self)
        self.connection = None
        self.ping_counter = 1
        self.last_pong = msg_pong(0)
        self.sleep_time = 0.1

    def add_connection(self, conn):
        self.connection = conn

    # Wrapper for the NodeConn's send_message function
    def send_message(self, message):
        self.connection.send_message(message)

    def on_pong(self, conn, message):
        self.last_pong = message

    # Syncing helpers
    def sync(self, test_function, timeout=30):
        while timeout > 0:
            with mininode_lock:
                if test_function():
                    return
            time.sleep(self.sleep_time)
            timeout -= self.sleep_time
        raise AssertionError("Sync failed to complete")

    def sync_with_ping(self):
        self.send_message(msg_ping(nonce=self.ping_counter))
        test_function = lambda: self.last_pong.nonce == self.ping_counter
        self.sync(test_function)
        self.ping_counter += 1
        return


class TestNode(BaseNode):
    def __init__(self):
        BaseNode.__init__(self)
        self.txids = []
        self.txidstream_isopen = False
        self.txidstream_pos = 0

    def on_inv(self, conn, message):
        if self.txidstream_isopen:
            for i in message.inv:
                if i.type == 1:
                    self.txids.append(('%x' % i.hash).zfill(64))

    def open_txidstream(self):
        self.txidstream_isopen = True

    def read_txidstream(self):
        if not self.txidstream_isopen:
            raise AssertionError("TXID stream not opened for reading")
        self.sync(lambda: len(self.txids) >= self.txidstream_pos + 1)
        self.txidstream_pos += 1
        return self.txids[self.txidstream_pos - 1]


class SPVTest(BitcoinTestFramework):

    def __init__(self):
        self.num_nodes = 2

    def setup_chain(self):
        print("Initializing test directory "+self.options.tmpdir)
        initialize_chain_clean(self.options.tmpdir, self.num_nodes)

    def setup_network(self):
        self.nodes = []
        self.nodes.append(start_node(0, self.options.tmpdir, ["-allowfreetx=0 -debug=mempool"]))
        self.nodes.append(start_node(1, self.options.tmpdir, ["-allowfreetx=0 -debug=mempool"]))
        connect_nodes(self.nodes[0], 1)

    def run_test(self):
        # Setup the p2p connections
        spv_node = TestNode()
        connection = NodeConn('127.0.0.1', p2p_port(0), self.nodes[0], spv_node, services=0)
        spv_node.add_connection(connection)

        # Add a bunch of extra connections to our nodes[0] peer, so spv_node is
        # unlikely to get inv's due to trickling rather than filter matches
        other_nodes = []
        for i in range(0,25):
            other_nodes.append(BaseNode())
            other_connection = NodeConn('127.0.0.1', p2p_port(0), self.nodes[0], other_nodes[i])
            other_nodes[i].add_connection(other_connection)

        NetworkThread().start() # Start up network handling in another thread
        spv_node.wait_for_verack()

        # Generate some coins
        self.nodes[1].generate(110)
        sync_blocks(self.nodes[0:2])

        # Start collecting txid inv's
        spv_node.open_txidstream()

        # Generate an address and extract pubkeyhash
        address0 = self.nodes[0].getnewaddress()
        dummyoutputs = {}
        dummyoutputs[address0] = 1
        dummytxhex = self.nodes[0].createrawtransaction([], dummyoutputs)
        dummytx = self.nodes[0].decoderawtransaction(dummytxhex)
        dummyasm = dummytx["vout"][0]["scriptPubKey"]["asm"]
        pkhstart = dummyasm.index("OP_HASH160") + len("OP_HASH160") + 1
        pkhhex = dummyasm[pkhstart:pkhstart+20*2]
        pubkeyhash0 = bytearray.fromhex(pkhhex)

        # Load bloom filter to peer
        spvFilter = CBloomFilter(nFlags=CBloomFilter.ANCESTOR_UPDATE_BIT)
        spvFilter.insert(pubkeyhash0)
        spv_node.send_message(msg_filterload(spvFilter))

        # Test 1. Bloom filter positive match
        tx1_id = self.nodes[1].sendtoaddress(address0, 1)
        got_txid = spv_node.read_txidstream()
        assert_equal(got_txid, tx1_id) #tx1 pays us

        # Test 2. Ancestor relay and mempool response

        # Send a control tx that neither pays us, nor is an ancestor of a tx that pays us
        txextra_id = self.nodes[1].sendtoaddress(self.nodes[1].getnewaddress(), 15)

        # Build an ancestor chain where the grandchild pays us
        tx2grandparent_id = self.nodes[1].sendtoaddress(self.nodes[1].getnewaddress(), 25)

        tx2parent_input = {}
        tx2parent_input["txid"] = tx2grandparent_id
        tx2parent_input["vout"] = 0
        tx2parent_output = {}
        tx2parent_output[self.nodes[1].getnewaddress()] = 12.5
        tx2parent_output[self.nodes[1].getnewaddress()] = 12.48
        tx2parent = self.nodes[1].createrawtransaction([tx2parent_input], tx2parent_output)
        tx2parentsignresult = self.nodes[1].signrawtransaction(tx2parent)
        assert_equal(tx2parentsignresult["complete"], True)
        tx2parent_id = self.nodes[1].sendrawtransaction(tx2parentsignresult["hex"])

        # Match tx2 by its consumption of a specific UTXO
        spvFilter.insert(COutPoint(int(tx2parent_id,16),0))
        spv_node.send_message(msg_filterload(spvFilter))

        tx2_input = {}
        tx2_input["txid"] = tx2parent_id
        tx2_input["vout"] = 0
        tx2_output = {}
        tx2_output[self.nodes[0].getnewaddress()] = 2
        tx2_output[self.nodes[1].getnewaddress()] = 10.48
        tx2 = self.nodes[1].createrawtransaction([tx2_input], tx2_output)
        tx2signresult = self.nodes[1].signrawtransaction(tx2)
        assert_equal(tx2signresult["complete"], True)
        tx2_id = self.nodes[1].sendrawtransaction(tx2signresult["hex"])

        # Check that tx2 as well as all its ancestors are pushed to our SPV node
        relayed = [spv_node.read_txidstream(), spv_node.read_txidstream(), spv_node.read_txidstream()]
        expectedRelay = [tx2grandparent_id, tx2parent_id, tx2_id]
        assert_equal(sorted(relayed), sorted(expectedRelay))

        sync_mempools(self.nodes[0:2])
        spv_node.send_message(msg_mempool())

        # Check the complete filtered mempool returned by our peer
        pool = [spv_node.read_txidstream(), spv_node.read_txidstream(), spv_node.read_txidstream(), spv_node.read_txidstream()]
        expectedPool = [tx1_id, tx2grandparent_id, tx2parent_id, tx2_id]
        assert_equal(sorted(pool), sorted(expectedPool))

if __name__ == '__main__':
    SPVTest().main()
