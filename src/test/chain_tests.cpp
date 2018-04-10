// Copyright (c) 2018 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "chain.h"
#include "test/test_bitcoin.h"

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

BOOST_AUTO_TEST_SUITE_END();
