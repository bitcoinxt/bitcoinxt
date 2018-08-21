// Copyright (c) 2017 - 2018 The Bitcoin developers
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

BOOST_AUTO_TEST_CASE(is_thirdhf_active) {
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

    arg->Set("-uahftime", 1);
    // Activation time is exactly mtp of block2.
    // In this test block2 and block3 have the same mtp.
    arg->Set("-thirdhftime", block2.GetMedianTimePast());

    BOOST_CHECK(!IsThirdHFActive(genesis.GetMedianTimePast()));
    BOOST_CHECK(IsThirdHFActive(block2.GetMedianTimePast()));
    BOOST_CHECK(IsThirdHFActive(block3.GetMedianTimePast()));
    BOOST_CHECK(IsThirdHFActive(block4.GetMedianTimePast()));

    // Never active if mayhf is disabled.
    arg->Set("-thirdhftime", 0);
    BOOST_CHECK(!IsThirdHFActive(genesis.GetMedianTimePast()));
    BOOST_CHECK(!IsThirdHFActive(block2.GetMedianTimePast()));
    BOOST_CHECK(!IsThirdHFActive(block3.GetMedianTimePast()));
    BOOST_CHECK(!IsThirdHFActive(block4.GetMedianTimePast()));
}

BOOST_AUTO_TEST_CASE(is_fourthhf_active) {
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

    arg->Set("-uahftime", 1);
    // Activation time is exactly mtp of block2.
    // In this test block2 and block3 have the same mtp.
    arg->Set("-fourthhftime", block2.GetMedianTimePast());

    BOOST_CHECK(!IsFourthHFActive(genesis.GetMedianTimePast()));
    BOOST_CHECK(IsFourthHFActive(block2.GetMedianTimePast()));
    BOOST_CHECK(IsFourthHFActive(block3.GetMedianTimePast()));
    BOOST_CHECK(IsFourthHFActive(block4.GetMedianTimePast()));

    // Never active if novemberhf is disabled.
    arg->Set("-fourthhftime", 0);
    BOOST_CHECK(!IsFourthHFActive(genesis.GetMedianTimePast()));
    BOOST_CHECK(!IsFourthHFActive(block2.GetMedianTimePast()));
    BOOST_CHECK(!IsFourthHFActive(block3.GetMedianTimePast()));
    BOOST_CHECK(!IsFourthHFActive(block4.GetMedianTimePast()));
}

class DummyMempool : public CTxMemPool {
public:
    DummyMempool() : CTxMemPool(CFeeRate(0)) { }
    void clear() override { clearCalls++; }
    int clearCalls = 0;
};

BOOST_AUTO_TEST_CASE(forkmempoolclearer_nullptr) {
    DummyMempool mempool;
    CBlockIndex tip;
    ForkMempoolClearer(mempool, nullptr, nullptr);
    ForkMempoolClearer(mempool, &tip, nullptr);
    ForkMempoolClearer(mempool, nullptr, &tip);
    BOOST_CHECK_EQUAL(0, mempool.clearCalls);
}

BOOST_AUTO_TEST_CASE(forkmempoolclearer_uahf) {
    CBlockIndex oldTip;
    CBlockIndex newTip;

    oldTip.nTime = Opt().UAHFTime() - 1;
    oldTip.nHeight = 0;
    newTip.nTime = Opt().UAHFTime();
    newTip.nHeight = 1;
    newTip.pprev = &oldTip;
    BOOST_CHECK(IsUAHFActivatingBlock(newTip.GetMedianTimePast(), &oldTip));

    // this fork adds replay protection, so the mempool must be cleared both
    // going into the fork and rollbacking from it

    DummyMempool mempool;
    ForkMempoolClearer(mempool, &oldTip, &oldTip);
    BOOST_CHECK_EQUAL(0, mempool.clearCalls);
    // fork time
    ForkMempoolClearer(mempool, &oldTip, &newTip);
    BOOST_CHECK_EQUAL(1, mempool.clearCalls);
    // rollback
    ForkMempoolClearer(mempool, &newTip, &oldTip);
    BOOST_CHECK_EQUAL(2, mempool.clearCalls);
    // past fork time
    oldTip.nTime = Opt().UAHFTime();
    newTip.nTime = Opt().UAHFTime() + 1;
    BOOST_CHECK_EQUAL(2, mempool.clearCalls);
}

BOOST_AUTO_TEST_CASE(forkmempoolclearer_thirdhf) {
    CBlockIndex oldTip;
    CBlockIndex newTip;

    oldTip.nTime = Opt().ThirdHFTime() - 1;
    oldTip.nHeight = 0;
    newTip.nTime = Opt().ThirdHFTime();
    newTip.nHeight = 1;
    newTip.pprev = &oldTip;
    BOOST_CHECK(IsThirdHFActivatingBlock(newTip.GetMedianTimePast(), &oldTip));

    // this fork adds new op codes, so the mempool can be kept when
    // going into the fork, but must be cleared when rollbacking from it

    DummyMempool mempool;
    ForkMempoolClearer(mempool, &oldTip, &oldTip);
    BOOST_CHECK_EQUAL(0, mempool.clearCalls);
    // fork time
    ForkMempoolClearer(mempool, &oldTip, &newTip);
    BOOST_CHECK_EQUAL(0, mempool.clearCalls);
    // rollback
    ForkMempoolClearer(mempool, &newTip, &oldTip);
    BOOST_CHECK_EQUAL(1, mempool.clearCalls);
    // past fork time
    oldTip.nTime = Opt().ThirdHFTime();
    newTip.nTime = Opt().ThirdHFTime() + 1;
    BOOST_CHECK_EQUAL(1, mempool.clearCalls);
}


BOOST_AUTO_TEST_SUITE_END()
