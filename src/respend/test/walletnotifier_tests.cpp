// Copyright (c) 2018 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "random.h"
#include "respend/walletnotifier.h"
#include "test/test_bitcoin.h"
#include "validationinterface.h"

#include <boost/test/unit_test.hpp>

using namespace respend;

BOOST_FIXTURE_TEST_SUITE(respend_walletnotifier_tests, BasicTestingSetup);

BOOST_AUTO_TEST_CASE(not_interesting) {
    WalletNotifier w;
    BOOST_CHECK(!w.IsInteresting());
    CTxMemPool::txiter dummy;
    bool lookAtMore;

    lookAtMore = w.AddOutpointConflict(COutPoint{}, dummy, CTransaction{},
                                            true /* seen before */, false, true);
    BOOST_CHECK(lookAtMore);
    BOOST_CHECK(!w.IsInteresting());

    lookAtMore = w.AddOutpointConflict(COutPoint{}, dummy, CTransaction{},
                                       false, true /* is equivalent */, true);
    BOOST_CHECK(lookAtMore);
    BOOST_CHECK(!w.IsInteresting());
}

BOOST_AUTO_TEST_CASE(is_interesting) {
    WalletNotifier w;
    CTxMemPool::txiter dummy;
    bool lookAtMore;

    lookAtMore = w.AddOutpointConflict(COutPoint{}, dummy, CTransaction{}, false, false, true);
    BOOST_CHECK(!lookAtMore);
    BOOST_CHECK(w.IsInteresting());
}

BOOST_AUTO_TEST_CASE(triggers_correctly) {
    CTxMemPool mempool(CFeeRate(0));
    CTxMemPool::setEntries setEntries;
    CTransaction slotTx;
    bool slotRespend = false;
    int slotCalls = 0;
    auto dummyslot = [&slotCalls, &slotTx, &slotRespend](const CTransaction& tx, const CBlock*, bool respend) {
        slotCalls++;
        slotTx = tx;
        slotRespend = respend;
    };
    GetMainSignals().SyncTransaction.connect(dummyslot);

    CTxMemPool::txiter dummy;
    CMutableTransaction respend;
    respend.vin.resize(1);
    respend.vin[0].prevout.n = 0;
    respend.vin[0].prevout.hash = GetRandHash();
    respend.vin[0].scriptSig << OP_1;

    // Create a "not interesting" respend
    WalletNotifier w;
    w.AddOutpointConflict(COutPoint{}, dummy, respend, true, false, true);
    w.OnFinishedTrigger();
    BOOST_CHECK_EQUAL(0, slotCalls);
    w.OnValidTrigger(true, mempool, setEntries);
    w.OnFinishedTrigger();
    BOOST_CHECK_EQUAL(0, slotCalls);

    // Create an interesting, but invalid respend
    w.AddOutpointConflict(COutPoint{}, dummy, respend, false, false, true);
    BOOST_CHECK(w.IsInteresting());
    w.OnValidTrigger(false, mempool, setEntries);
    w.OnFinishedTrigger();
    BOOST_CHECK_EQUAL(0, slotCalls);
    // make valid
    w.OnValidTrigger(true, mempool, setEntries);
    w.OnFinishedTrigger();
    BOOST_CHECK_EQUAL(1, slotCalls);
    BOOST_CHECK(slotRespend);
    BOOST_CHECK(CTransaction(respend) == slotTx);
    GetMainSignals().SyncTransaction.disconnect_all_slots();
}

BOOST_AUTO_TEST_SUITE_END();
