#!/usr/bin/env python3
# Copyright (c) 2015 The Bitcoin Core developers
# Copyright (c) 2015-2017 The Bitcoin Unlimited developers
# Copyright (c) 2017 The Bitcoin developers
# Distributed under the MIT/X11 software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
#
import sys
import logging
from test_framework.util import sync_blocks
from test_framework.test_framework import ComparisonTestFramework
from test_framework.util import *
from test_framework.mininode import CTransaction, NetworkThread
from test_framework.blocktools import create_coinbase, create_block
from test_framework.comptool import TestInstance, TestManager
from test_framework.script import CScript, OP_1NEGATE, OP_CHECKSEQUENCEVERIFY, OP_DROP
from io import BytesIO
import time
import itertools
import tempfile

'''
This test exercises BIP135 fork grace periods that check the activation timeout
using a single node.
'''

VERSIONBITS_TOP_BITS = 0x20000000

class BIP135ForksTest(ComparisonTestFramework):

    def __init__(self):
        super().__init__()
        self.setup_clean_chain = True
        self.num_nodes = 1
        self.defined_forks = [ "bip135test%d" % i for i in range(0,25) ]
        self.first_test = 23

    def setup_network(self):
        # blocks are 1 second apart by default in this regtest
        # regtest bip135 deployments are defined for a blockchain that starts at MOCKTIME
        enable_mocktime()
        # starttimes are offset by 50 seconds from init_time
        self.fork_starttime = get_mocktime() + 30
        self.nodes = start_nodes(1, self.options.tmpdir,
                                 extra_args=[['-debug=all', '-whitelist=127.0.0.1']],
                                 binary=[self.options.testbinary])

    def run_test(self):
        self.test = TestManager(self, self.options.tmpdir)
        self.test.add_all_connections(self.nodes)
        NetworkThread().start() # Start up network handling in another thread
        self.test.run()

    def generate_blocks(self, number, version, test_blocks = []):
        for i in range(number):
            self.height += 1
            self.last_block_time += 1
            block = create_block(self.tip, create_coinbase(absoluteHeight=self.height), self.last_block_time)
            block.nVersion = version
            block.rehash()
            block.solve()
            test_blocks.append([block, True])
            self.tip = block.sha256
        return test_blocks

    def print_rpc_status(self):
        for f in self.defined_forks[self.first_test:]:
            info = self.nodes[0].getblockchaininfo()
            logging.info(info['bip135_forks'][f])

    def test_BIP135GraceConditions(self):
        logging.basicConfig(format='%(asctime)s.%(levelname)s: %(message)s',
                    level=logging.INFO,
                    stream=sys.stdout)

        logging.info("begin test_BIP135GraceConditions test")
        node = self.nodes[0]
        self.tip = int("0x" + node.getbestblockhash(), 0)
        header = node.getblockheader("0x%x" % self.tip)
        assert_equal(header['height'], 0)

        # Test 1
        # generate a block and test addition of another
        self.coinbase_blocks = node.generate(1)
        self.height = 1
        self.last_block_time = get_mocktime()
        self.tip = int("0x" + node.getbestblockhash(), 0)
        test_blocks = self.generate_blocks(1, VERSIONBITS_TOP_BITS)
        yield TestInstance(test_blocks, sync_every_block=False)

        bcinfo = node.getblockchaininfo()
        for f in self.defined_forks[self.first_test:]:
            assert_equal(bcinfo['bip135_forks'][f]['bit'], int(f[10:]))
            assert_equal(bcinfo['bip135_forks'][f]['status'], 'defined')

        # move to starttime
        bcinfo = node.getblockchaininfo()
        tip_mediantime = int(bcinfo['mediantime'])
        while tip_mediantime < self.fork_starttime or self.height % 10 != 9:
            test_blocks = self.generate_blocks(1, VERSIONBITS_TOP_BITS)
            yield TestInstance(test_blocks, sync_every_block=False)
            bcinfo = node.getblockchaininfo()
            tip_mediantime = int(bcinfo['mediantime'])
        for f in self.defined_forks[self.first_test:]:
            assert_equal(bcinfo['bip135_forks'][f]['status'], 'started')

        # Lock one of them them in by producing 8 signaling blocks out of 10.
        # The one that is not locked in will be one block away from lock in.
        test_blocks = self.generate_blocks(8, 0x21800000)
        # last two blocks don't need to signal
        test_blocks = self.generate_blocks(2, VERSIONBITS_TOP_BITS, test_blocks)
        yield TestInstance(test_blocks, sync_every_block=False)
        # check bits 23-24, only 24 should be LOCKED_IN
        bcinfo = node.getblockchaininfo()
        logging.info("checking all grace period forks are locked in")
        activation_states = [ bcinfo['bip135_forks'][f]['status'] for f in self.defined_forks[self.first_test:] ]
        assert_equal(activation_states, ['started',
                                         'locked_in'
                                         ])

        # now we just check that they turn ACTIVE only when their configured
        # conditions are all met. Reminder: window size is 10 blocks, inter-
        # block time is 1 sec for the synthesized chain.
        #
        # Grace conditions for the bits 23-24:
        # -----------------------------------
        # bit 23:  minlockedblocks= 5, minlockedtime= 0  -> started next sync
        # bit 24:  minlockedblocks= 5, minlockedtime= 0  -> activate next sync

        # check the forks supposed to activate just one period after lock-in ("at next sync")
        # and move the time to just before timeout. Bit 24 should become active and neither should fail.
        # Set the last block time to 6 seconds before the timeout...since blocks get mined one second
        # apart this will put the MTP at 1 second behind the timeout, and thus the activation will not fail.
        self.last_block_time = self.fork_starttime + 50 - 6

        test_blocks = self.generate_blocks(10, VERSIONBITS_TOP_BITS)
        yield TestInstance(test_blocks, sync_every_block=False)
        bcinfo = node.getblockchaininfo()
        activation_states = [ bcinfo['bip135_forks'][f]['status'] for f in self.defined_forks[self.first_test:] ]
        assert_equal(activation_states, ['started',
                                         'active'
                                         ])


        # Move the time to the timeout. Bit 23 never locked in and should be 'failed',
        # whereas, Bit 24 should still be active.
        test_blocks = self.generate_blocks(10, VERSIONBITS_TOP_BITS)
        yield TestInstance(test_blocks, sync_every_block=False)
        bcinfo = node.getblockchaininfo()
        activation_states = [ bcinfo['bip135_forks'][f]['status'] for f in self.defined_forks[self.first_test:] ]
        assert_equal(activation_states, ['failed',
                                         'active'
                                         ])

    def get_tests(self):
        '''
        run various tests
        '''
        # CSV (bit 0) for backward compatibility with BIP9
        for test in itertools.chain(
                self.test_BIP135GraceConditions(), # test grace periods
        ):
            yield test



if __name__ == '__main__':
    BIP135ForksTest().main()
