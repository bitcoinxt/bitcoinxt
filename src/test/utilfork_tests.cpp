// Copyright (c) 2017 The Bitcoin XT developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#include "chain.h"
#include "chainparams.h"
#include "options.h"
#include "test/test_bitcoin.h"
#include "utilfork.h"

#include <limits.h>
#include <iostream>

#include <boost/test/unit_test.hpp>

namespace {
bool isActivating(const CBlockIndex& b) {
    return IsUAHFActivatingBlock(b.GetMedianTimePast(), b.pprev);
}

} // ns anon

BOOST_FIXTURE_TEST_SUITE(utilfork_tests, BasicTestingSetup);

BOOST_AUTO_TEST_CASE(is_uahf_activating_block_at_genesis) {

    auto arg = new DummyArgGetter;
    auto argraii = SetDummyArgGetter(std::unique_ptr<ArgGetter>(arg));

    CBlockIndex genesis;
    genesis.pprev = nullptr;
    genesis.nTime = 11;

    // UAHF activates after genesis
    arg->Set("-uahftime", genesis.GetMedianTimePast() + 1);
    BOOST_CHECK(!isActivating(genesis));

    // UAHF activates on genesis
    arg->Set("-uahftime", genesis.GetMedianTimePast());
    BOOST_CHECK(isActivating(genesis));

    // UAHF is disabled.
    arg->Set("-uahftime", 0);
    BOOST_CHECK(!isActivating(genesis));
}

BOOST_AUTO_TEST_CASE(is_uahf_activating_block_normal) {
    // normal case, activation is somewhere in the chain (not at genesis)

    auto arg = new DummyArgGetter;
    auto argraii = SetDummyArgGetter(std::unique_ptr<ArgGetter>(arg));

    CBlockIndex genesis;
    genesis.pprev = nullptr;
    genesis.nTime = 11;

    CBlockIndex block2;
    block2.pprev = &genesis;
    block2.nTime = 42;

    CBlockIndex block3;
    block3.pprev = &block2;
    block3.nTime = 50;

    CBlockIndex block4;
    block4.pprev = &block3;
    block4.nTime = 100;

    // Activation time is exactly mtp of block2.
    // In this test block2 and block3 have the same mtp.
    arg->Set("-uahftime", block2.GetMedianTimePast());

    BOOST_CHECK(!isActivating(genesis));
    BOOST_CHECK(isActivating(block2));
    BOOST_CHECK(!isActivating(block3));
    BOOST_CHECK(!isActivating(block4));

    // Activation time is before mtp of block2 (but after mtp of genesis)
    arg->Set("-uahftime", block2.GetMedianTimePast() - 1);
    BOOST_CHECK(!isActivating(genesis));
    BOOST_CHECK(isActivating(block2));
    BOOST_CHECK(!isActivating(block3));
    BOOST_CHECK(!isActivating(block4));

    // No activation yet.
    arg->Set("-uahftime", block4.GetMedianTimePast() + 1);
    BOOST_CHECK(!isActivating(genesis));
    BOOST_CHECK(!isActivating(block2));
    BOOST_CHECK(!isActivating(block3));
    BOOST_CHECK(!isActivating(block4));
}

BOOST_AUTO_TEST_SUITE_END()
