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
This test exercises BIP135 fork activation thresholds using a single node.
'''

VERSIONBITS_TOP_BITS = 0x20000000

class BIP135ForksTest(ComparisonTestFramework):

    def __init__(self):
        super().__init__()
        self.setup_clean_chain = True
        self.num_nodes = 1
        self.defined_forks = [ "bip135test%d" % i for i in range(0,8) ]
        self.first_test = 1

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
            old_tip = self.tip
            self.height += 1
            self.last_block_time += 1
            block = create_block(self.tip, create_coinbase(absoluteHeight=self.height), self.last_block_time)
            block.nVersion = version
            block.rehash()
            block.solve()
            test_blocks.append([block, True])
            self.tip = block.sha256
            #logging.info ("generate_blocks: created block %x on tip %x, height %d, time %d" % (self.tip, old_tip, self.height-1, self.last_block_time))
        return test_blocks

    def print_rpc_status(self):
        for f in self.defined_forks[self.first_test:]:
            info = self.nodes[0].getblockchaininfo()
            logging.info(info['bip135_forks'][f])

    def test_BIP135Thresholds(self):
        logging.basicConfig(format='%(asctime)s.%(levelname)s: %(message)s',
                    level=logging.INFO,
                    stream=sys.stdout)

        logging.info("test_BIP135Thresholds: begin")
        node = self.nodes[0]
        self.tip = int("0x" + node.getbestblockhash(), 0)
        header = node.getblockheader("0x%x" % self.tip)
        assert_equal(header['height'], 0)
        bcinfo = node.getblockchaininfo()
        for f in self.defined_forks[self.first_test:]:
            assert_equal(bcinfo['bip135_forks'][f]['bit'], int(f[10:]))

        # Test 1
        # generate a block and test addition of another
        self.coinbase_blocks = node.generate(1)
        self.height = 1
        self.last_block_time = get_mocktime()
        self.tip = int("0x" + node.getbestblockhash(), 0)
        test_blocks = self.generate_blocks(1, VERSIONBITS_TOP_BITS)  # do not set bit 1 yet
        yield TestInstance(test_blocks, sync_every_block=False)

        # Test 2
        # check initial DEFINED state
        # check initial forks status and getblocktemplate
        logging.info("begin test 2")
        tmpl = node.getblocktemplate({})
        tip_mediantime = int(tmpl['mintime']) - 1
        assert_equal(tmpl['vbrequired'], 0)
        assert_equal(tmpl['version'], VERSIONBITS_TOP_BITS)
        logging.info("initial getblocktemplate:\n%s" % tmpl)

        test_blocks = self.generate_blocks(96, 0x20000001)
        yield TestInstance(test_blocks, sync_every_block=False)

        while tip_mediantime < self.fork_starttime or self.height < (100 - 1):
            for f in self.defined_forks[self.first_test:]:
                assert_equal(get_bip135_status(node, f)['status'], 'defined')
                assert(f not in tmpl['rules'])
                assert(f not in tmpl['vbavailable'])
            test_blocks = self.generate_blocks(1, 0x20000001)
            yield TestInstance(test_blocks, sync_every_block=False)
            tmpl = node.getblocktemplate({})
            tip_mediantime = int(tmpl['mintime']) - 1

        # Test 3
        # Advance from DEFINED to STARTED
        logging.info("begin test 3")
        bcinfo = node.getblockchaininfo()
        for f in self.defined_forks[self.first_test:]:
            if int(f[10:]) > 1:
                assert_equal(bcinfo['bip135_forks'][f]['status'], 'started')
                assert(f not in tmpl['rules'])
                assert(f in tmpl['vbavailable'])
            elif int(f[10:]) == 1: # bit 1 only becomes started at height 144
                assert_equal(bcinfo['bip135_forks'][f]['status'], 'defined')

        # Test 4
        # Advance from DEFINED to STARTED
        logging.info("begin test 4")
        test_blocks = self.generate_blocks(42, 0x20000001)
        yield TestInstance(test_blocks, sync_every_block=False)
        # move up until it starts
        while self.height < (144 - 1):
            for f in self.defined_forks[self.first_test:]:
                if int(f[10:]) > 1:
                    assert_equal(bcinfo['bip135_forks'][f]['status'], 'started')
                    assert(f not in tmpl['rules'])
                    assert(f in tmpl['vbavailable'])
                else: # bit 1 only becomes started at height 144
                    assert_equal(bcinfo['bip135_forks'][f]['status'], 'defined')
            test_blocks = self.generate_blocks(1, 0x20000001)
            yield TestInstance(test_blocks, sync_every_block=False)
            bcinfo = node.getblockchaininfo()
            tmpl = node.getblocktemplate({})

        # now it should be started
        assert_equal(bcinfo['bip135_forks'][self.defined_forks[1]]['status'], 'started')
        assert(self.defined_forks[1] not in tmpl['rules'])
        assert_equal(tmpl['vbavailable'][self.defined_forks[1]], 1)
        assert_equal(tmpl['vbrequired'], 0)
        assert(tmpl['version'] & VERSIONBITS_TOP_BITS + 2**1)

        # Test 5
        # Lock-in
        logging.info("begin test 5")

        # move to start of new 100-block window
        test_blocks = self.generate_blocks((100 - 1) - (self.height % 100), 0x20000003)
        yield TestInstance(test_blocks, sync_every_block=False)
        assert(self.height % 100 == 99)

        # generate enough of bits 2-7 in next 100 blocks to lock in fork bits 2-7
        # bit 1 will only be locked in at next multiple of 144
        # 1 block total for bit 2
        test_blocks = self.generate_blocks(1, 0x200000FD)
        yield TestInstance(test_blocks, sync_every_block=False)
        # check still STARTED until we get to multiple of window size
        assert_equal(get_bip135_status(node, self.defined_forks[1])['status'], 'started')

        # 10 blocks total for bit 3
        test_blocks = self.generate_blocks(9, 0x200000F9)
        yield TestInstance(test_blocks, sync_every_block=False)
        assert_equal(get_bip135_status(node, self.defined_forks[2])['status'], 'started')

        # 75 blocks total for bit 4
        test_blocks = self.generate_blocks(65, 0x200000F1)
        yield TestInstance(test_blocks, sync_every_block=False)
        assert_equal(get_bip135_status(node, self.defined_forks[3])['status'], 'started')

        # 95 blocks total for bit 5
        test_blocks = self.generate_blocks(20, 0x200000E1)
        yield TestInstance(test_blocks, sync_every_block=False)
        assert_equal(get_bip135_status(node, self.defined_forks[4])['status'], 'started')

        # 99 blocks total for bit 6
        test_blocks = self.generate_blocks(4, 0x200000C1)
        yield TestInstance(test_blocks, sync_every_block=False)
        assert_equal(get_bip135_status(node, self.defined_forks[5])['status'], 'started')

        # 100 blocks total for bit 7
        test_blocks = self.generate_blocks(1, 0x20000081)
        yield TestInstance(test_blocks, sync_every_block=False)
        assert(self.height % 100 == 99)

        bcinfo = node.getblockchaininfo()
        for f in self.defined_forks[2:]:
            assert_equal(bcinfo['bip135_forks'][f]['status'], 'locked_in')
        assert_equal(bcinfo['bip135_forks'][self.defined_forks[1]]['status'], 'started')

        # move to start of new 144-block window
        test_blocks = self.generate_blocks((144 - 1) - (self.height % 144), 0x20000003)
        yield TestInstance(test_blocks, sync_every_block=False)
        assert(self.height % 144 == 143)
        bcinfo = node.getblockchaininfo()
        assert_equal(bcinfo['bip135_forks'][self.defined_forks[1]]['status'], 'locked_in')

        # Test 6
        # Activation
        logging.info("begin test 6")

        for f in self.defined_forks[2:]:
            assert_equal(bcinfo['bip135_forks'][f]['status'], 'active')

    def get_tests(self):
        '''
        run various tests
        '''
        # CSV (bit 0) for backward compatibility with BIP9
        for test in itertools.chain(
                self.test_BIP135Thresholds(),  # test thresholds on other bits
        ):
            yield test



if __name__ == '__main__':
    BIP135ForksTest().main()
