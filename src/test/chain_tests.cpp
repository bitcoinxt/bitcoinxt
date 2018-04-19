// Copyright (c) 2018 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "chain.h"
#include "test/test_bitcoin.h"
#include "consensus/consensus.h" // for MAX_BLOCK_SIZE

#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(cchain_tests, BasicTestingSetup);

BOOST_AUTO_TEST_CASE(chaintip_observer) {

    CChain chain;

    const CBlockIndex* oldTip = nullptr;
    const CBlockIndex* newTip = nullptr;
    int callbacks = 0;

    chain.AddTipObserver([&](const CBlockIndex* o, const CBlockIndex* n) {
            oldTip = o;
            newTip = n;
            ++callbacks;
        });

    CBlockIndex a, b;
    a.nHeight = 0;
    b.nHeight = 1;
    b.pprev = &a;

    chain.SetTip(&b);
    BOOST_CHECK(oldTip == nullptr);
    BOOST_CHECK(newTip == &b);
    BOOST_CHECK_EQUAL(1, callbacks);

    CBlockIndex c;
    b.nHeight = 0;
    chain.SetTip(&c);
    BOOST_CHECK(oldTip == &b);
    BOOST_CHECK(newTip == &c);
    BOOST_CHECK_EQUAL(2, callbacks);

    chain.SetTip(nullptr);
    BOOST_CHECK(oldTip == &c);
    BOOST_CHECK(newTip == nullptr);
    BOOST_CHECK_EQUAL(3, callbacks);
}

BOOST_AUTO_TEST_CASE(tip_max_blocksize) {
    CChain chain;

    // no tip set
    BOOST_CHECK_EQUAL(MAX_BLOCK_SIZE, chain.MaxBlockSizeInsecure());

    // set tips
    CBlockIndex a;
    a.nHeight = 0;
    a.nMaxBlockSize = MAX_BLOCK_SIZE * 1;

    CBlockIndex b;
    b.nHeight = 1;
    b.nMaxBlockSize = MAX_BLOCK_SIZE * 2;
    b.pprev = &a;

    CBlockIndex c;
    c.nHeight = 0;
    c.nMaxBlockSize = MAX_BLOCK_SIZE * 3;

    chain.SetTip(&b);
    BOOST_CHECK_EQUAL(2 * MAX_BLOCK_SIZE, chain.MaxBlockSizeInsecure());
    chain.SetTip(&c);
    BOOST_CHECK_EQUAL(3 * MAX_BLOCK_SIZE, chain.MaxBlockSizeInsecure());
}

BOOST_AUTO_TEST_SUITE_END();
