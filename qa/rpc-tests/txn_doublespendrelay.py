#!/usr/bin/env python3

#
# Test double-spend-relay and notification code
#

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import *
from decimal import Decimal

SEQUENCE_IMMED_RELAY = 0xBFFFFFFF

class DoubleSpendRelay(BitcoinTestFramework):

    def __init__(self):
        super().__init__()
        self.num_nodes = 4
        self.setup_clean_chain = False

    #
    # Create a 4-node network; roles for the nodes are:
    # [0] : transaction creator
    # [1] : respend sender
    # [2] : relay node
    # [3] : receiver, should detect/notify of double-spends
    #
    # Node connectivity is:
    # [0,1] <--> [2] <--> [3]
    #
    def setup_network(self):
        #This test requires mocktime
        enable_mocktime()
        self.is_network_split = False
        self.nodes = []
        for i in range(0,4):
            self.nodes.append(start_node(i, self.options.tmpdir, ["-debug", "-replacebysi=1"]))
        connect_nodes(self.nodes[0], 2)
        connect_nodes(self.nodes[1], 2)
        connect_nodes(self.nodes[3], 2)
        return self.nodes

    def run_test(self):
        fee = Decimal("0.01")

        nodes = self.nodes
        # Test 1: First spend
        # shutdown nodes[1] so it is not aware of the first spend
        # and will be willing to broadcast a respend
        stop_node(nodes[1], 1)
        # First spend: nodes[0] -> nodes[3]
        amount = Decimal("49")
        (total_in, tx1_inputs) = gather_inputs(nodes[0], amount+fee)
        tx1_inputs[0]['sequence'] = SEQUENCE_IMMED_RELAY
        change_outputs = make_change(nodes[0], total_in, amount, fee)
        outputs = dict(change_outputs)
        outputs[nodes[3].getnewaddress()] = amount
        signed = nodes[0].signrawtransaction(nodes[0].createrawtransaction(tx1_inputs, outputs))
        txid1 = nodes[0].sendrawtransaction(signed["hex"], True)
        sync_mempools([nodes[0], nodes[3]])

        txid1_info = nodes[3].gettransaction(txid1)
        assert_equal(txid1_info["respendsobserved"], [])

        # Test 2: Is double-spend of tx1_inputs[0] relayed?
        # Restart nodes[1]
        nodes[1] = start_node(1, self.options.tmpdir, ["-debug", "-replacebysi=1"])
        connect_nodes(nodes[1], 2)
        # Second spend: nodes[0] -> nodes[0]
        amount = Decimal("40")
        total_in = Decimal("48")
        inputs2 = [tx1_inputs[0]]
        change_outputs = make_change(nodes[0], total_in, amount, fee)
        outputs = dict(change_outputs)
        outputs[nodes[0].getnewaddress()] = amount
        signed = nodes[0].signrawtransaction(nodes[0].createrawtransaction(inputs2, outputs))
        txid2 = nodes[1].sendrawtransaction(signed["hex"], True)
        # Wait until txid2 is relayed to nodes[3] (but don't wait forever):
        # Note we can't use sync_mempools, because the respend isn't added to
        # the mempool.
        def txid2_relay():
            return nodes[3].gettransaction(txid1)["respendsobserved"] != []
        wait_for(txid2_relay, what = "tx relay")
        txid1_info = nodes[3].gettransaction(txid1)
        assert_equal(txid1_info["respendsobserved"], [txid2])

        # Test 3: Is triple-spend of tx1_inputs[0] not relayed?
        # Clear node1 mempool
        stop_node(nodes[1], 1)
        nodes[1] = start_node(1, self.options.tmpdir)
        connect_nodes(nodes[1], 2)
        # Third spend: nodes[0] -> nodes[0]
        outputs = dict(change_outputs)
        outputs[nodes[0].getnewaddress()] = amount
        signed = nodes[0].signrawtransaction(nodes[0].createrawtransaction(inputs2, outputs))
        txid3 = nodes[1].sendrawtransaction(signed["hex"], True)
        # Ensure txid3 not relayed to nodes[3]:
        time.sleep(9.1)
        txid1_info = nodes[3].gettransaction(txid1)
        assert_equal(txid1_info["respendsobserved"], [txid2])

        # Test 4: txid5 respends non-SI txid4
        # txid5 should be relayed and replace txid4 in mempools
        stop_node(nodes[1], 1)

        # txid4 is a new first-spend. It is non-SI because it has multiple inputs
        amount = Decimal("99")
        (total_in, tx4_inputs) = gather_inputs(nodes[0], amount+fee)
        tx4_inputs[0]['sequence'] = SEQUENCE_IMMED_RELAY
        change_outputs = make_change(nodes[0], total_in, amount, fee)
        outputs = dict(change_outputs)
        outputs[nodes[3].getnewaddress()] = amount
        signed = nodes[0].signrawtransaction(nodes[0].createrawtransaction(tx4_inputs, outputs))
        txid4 = nodes[0].sendrawtransaction(signed["hex"], True)
        sync_mempools([nodes[0], nodes[3]])
        assert_equal(set(nodes[2].getrawmempool()), {txid1,txid4})

        nodes[1] = start_node(1, self.options.tmpdir, ["-debug", "-replacebysi=1"])
        connect_nodes(nodes[1], 2)

        # txid5 respends txid4
        amount = Decimal("41")
        total_in = Decimal("49")
        inputs5 = [tx4_inputs[0]]
        change_outputs = make_change(nodes[0], total_in, amount, fee)
        outputs = dict(change_outputs)
        outputs[nodes[0].getnewaddress()] = amount
        signed = nodes[0].signrawtransaction(nodes[0].createrawtransaction(inputs5, outputs))
        txid5 = nodes[1].sendrawtransaction(signed["hex"], True)
        def txid5_relay():
            return (set(nodes[3].getrawmempool()) != {txid1, txid4})
        wait_for(txid5_relay, what = "tx relay")
        txid4_info = nodes[3].gettransaction(txid4)
        assert_equal(txid4_info["respendsobserved"], [txid5])
        assert_equal(set(nodes[2].getrawmempool()), {txid1, txid5})

if __name__ == '__main__':
    DoubleSpendRelay().main()

