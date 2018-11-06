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
This test exercises BIP135 fork grace periods using a single node.
'''

VERSIONBITS_TOP_BITS = 0x20000000

class BIP135ForksTest(ComparisonTestFramework):

    def __init__(self):
        super().__init__()
        self.setup_clean_chain = True
        self.num_nodes = 1
        self.defined_forks = [ "bip135test%d" % i for i in range(0,23) ]
        self.first_test = 8

    def setup_network(self):
        # blocks are 1 second apart by default in this regtest
        # regtest bip135 deployments are defined for a blockchain that starts at MOCKTIME
        enable_mocktime()
        # starttimes are offset by 30 seconds from init_time
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

        # lock all of them them in by producing 9 signaling blocks out of 10
        test_blocks = self.generate_blocks(9, 0x207fff00)
        # tenth block doesn't need to signal
        test_blocks = self.generate_blocks(1, VERSIONBITS_TOP_BITS, test_blocks)
        yield TestInstance(test_blocks, sync_every_block=False)
        bcinfo = node.getblockchaininfo()
        logging.info("checking all grace period forks are locked in")
        for f in self.defined_forks[self.first_test:]:
            assert_equal(bcinfo['bip135_forks'][f]['status'], 'locked_in')

        # now we just check that they turn ACTIVE only when their configured
        # conditions are all met. Reminder: window size is 10 blocks, inter-
        # block time is 1 sec for the synthesized chain.
        #
        # Grace conditions for the bits 8-22:
        # -----------------------------------
        # bit 8:  minlockedblocks= 1, minlockedtime= 0  -> activate next sync
        # bit 9:  minlockedblocks= 5, minlockedtime= 0  -> activate next sync
        # bit 10:  minlockedblocks=10, minlockedtime= 0  -> activate next sync
        # bit 11: minlockedblocks=11, minlockedtime= 0  -> activate next plus one sync
        # bit 12: minlockedblocks= 0, minlockedtime= 1  -> activate next sync
        # bit 13: minlockedblocks= 0, minlockedtime= 5  -> activate next sync
        # bit 14: minlockedblocks= 0, minlockedtime= 9  -> activate next sync
        # bit 15: minlockedblocks= 0, minlockedtime=10  -> activate next sync
        # bit 16: minlockedblocks= 0, minlockedtime=11  -> activate next plus one sync
        # bit 17: minlockedblocks= 0, minlockedtime=15  -> activate next plus one sync
        # bit 18: minlockedblocks=10, minlockedtime=10  -> activate next sync
        # bit 19: minlockedblocks=10, minlockedtime=19  -> activate next plus one sync
        # bit 20: minlockedblocks=10, minlockedtime=20  -> activate next plus one sync
        # bit 21: minlockedblocks=20, minlockedtime=21  -> activate next plus two sync
        # bit 22: minlockedblocks=21, minlockedtime=20  -> activate next plus two sync

        # check the forks supposed to activate just one period after lock-in ("at next sync")

        test_blocks = self.generate_blocks(10, VERSIONBITS_TOP_BITS)
        yield TestInstance(test_blocks, sync_every_block=False)
        bcinfo = node.getblockchaininfo()
        activation_states = [ bcinfo['bip135_forks'][f]['status'] for f in self.defined_forks[self.first_test:] ]
        assert_equal(activation_states, ['active',
                                         'active',
                                         'active',
                                         'locked_in',
                                         'active',
                                         'active',
                                         'active',
                                         'active',
                                         'locked_in',
                                         'locked_in',
                                         'active',
                                         'locked_in',
                                         'locked_in',
                                         'locked_in',
                                         'locked_in'
                                         ])

        # check the ones supposed to activate at next+1 sync
        test_blocks = self.generate_blocks(10, VERSIONBITS_TOP_BITS)
        yield TestInstance(test_blocks, sync_every_block=False)
        bcinfo = node.getblockchaininfo()
        activation_states = [ bcinfo['bip135_forks'][f]['status'] for f in self.defined_forks[self.first_test:] ]
        assert_equal(activation_states, ['active',
                                         'active',
                                         'active',
                                         'active',
                                         'active',
                                         'active',
                                         'active',
                                         'active',
                                         'active',
                                         'active',
                                         'active',
                                         'active',
                                         'active',
                                         'locked_in',
                                         'locked_in'
                                         ])

        # check the ones supposed to activate at next+2 period
        test_blocks = self.generate_blocks(10, VERSIONBITS_TOP_BITS)
        yield TestInstance(test_blocks, sync_every_block=False)
        bcinfo = node.getblockchaininfo()
        activation_states = [ bcinfo['bip135_forks'][f]['status'] for f in self.defined_forks[self.first_test:] ]
        assert_equal(activation_states, ['active',
                                         'active',
                                         'active',
                                         'active',
                                         'active',
                                         'active',
                                         'active',
                                         'active',
                                         'active',
                                         'active',
                                         'active',
                                         'active',
                                         'active',
                                         'active',
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
