// Copyright (c) 2018 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "net.h" // for CConnman
#include "random.h"
#include "txmempool.h"
#include "primitives/transaction.h"
#include "respend/respendrelayer.h"
#include "script/standard.h"
#include "test/test_bitcoin.h"
#include "test/thinblockutil.h"


#include <boost/test/unit_test.hpp>

using namespace respend;

CMutableTransaction CreateRandomImmedTx() {
    CKey key;
    key.MakeNewKey(true);

    CMutableTransaction tx;
    tx.vin.resize(1);
    tx.vin[0].prevout.n = 0;
    tx.vin[0].prevout.hash = GetRandHash();
    tx.vin[0].scriptSig << OP_1;
    tx.vin[0].nSequence &= ~CTxIn::SEQUENCE_IMMED_RELAY_DISABLE_FLAG;
    tx.vout.resize(1);
    tx.vout[0].nValue = 1*CENT;
    tx.vout[0].scriptPubKey = GetScriptForDestination(key.GetPubKey().GetID());
    return tx;
}

BOOST_FIXTURE_TEST_SUITE(respendrelayer_tests, BasicTestingSetup);

BOOST_AUTO_TEST_CASE(not_interesting) {
    CConnman connman(0, 0);
    RespendRelayer r(&connman);
    BOOST_CHECK(!r.IsInteresting());
    CTxMemPool::txiter dummy;
    bool lookAtMore;

    lookAtMore = r.AddOutpointConflict(COutPoint{}, dummy, CTransaction{},
                                            true /* seen before */, false, true);
    BOOST_CHECK(lookAtMore);
    BOOST_CHECK(!r.IsInteresting());

    lookAtMore = r.AddOutpointConflict(COutPoint{}, dummy, CTransaction{},
                                       false, true /* is equivalent */, true);
    BOOST_CHECK(lookAtMore);
    BOOST_CHECK(!r.IsInteresting());
}

BOOST_AUTO_TEST_CASE(is_interesting) {
    CConnman connman(0, 0);
    RespendRelayer r(&connman);
    CTxMemPool mempool(CFeeRate(0));
    CMutableTransaction tx1 = CreateRandomImmedTx();
    TestMemPoolEntryHelper entry;
    mempool.addUnchecked(tx1.GetHash(), entry.FromTx(tx1, &mempool));
    CTxMemPool::txiter tx1Entry = mempool.mapTx.get<0>().begin();
    CMutableTransaction respend = CreateRandomImmedTx();
    bool lookAtMore;

    lookAtMore = r.AddOutpointConflict(COutPoint{}, tx1Entry, respend, false, false, true);
    BOOST_CHECK(!lookAtMore);
    BOOST_CHECK(r.IsInteresting());
}

BOOST_AUTO_TEST_CASE(triggers_correctly) {
    CTxMemPool mempool(CFeeRate(0));
    CMutableTransaction tx1 = CreateRandomImmedTx();
    TestMemPoolEntryHelper entry;
    mempool.addUnchecked(tx1.GetHash(), entry.FromTx(tx1, &mempool));
    CTxMemPool::txiter tx1Entry = mempool.mapTx.get<0>().begin();
    CMutableTransaction respend = CreateRandomImmedTx();

    DummyNode node;
    node.fRelayTxes = true;
    CConnman connman(0, 0);
    connman.AddTestNode(&node);

    // Create a "not interesting" respend
    RespendRelayer r(&connman);
    r.AddOutpointConflict(COutPoint{}, tx1Entry, respend, true, false, true);
    r.Trigger();
    BOOST_CHECK_EQUAL(size_t(0), node.vInventoryToSend.size());
    r.SetValid(true);
    r.Trigger();
    BOOST_CHECK_EQUAL(size_t(0), node.vInventoryToSend.size());

    // Create an interesting, but invalid respend
    r.AddOutpointConflict(COutPoint{}, tx1Entry, respend, false, false, true);
    BOOST_CHECK(r.IsInteresting());
    r.SetValid(false);
    r.Trigger();
    BOOST_CHECK_EQUAL(size_t(0), node.vInventoryToSend.size());
    // make valid
    r.SetValid(true);
    r.Trigger();
    BOOST_CHECK_EQUAL(size_t(1), node.vInventoryToSend.size());
    BOOST_CHECK(respend.GetHash() == node.vInventoryToSend.at(0).hash);
    connman.RemoveTestNode(&node);
}

BOOST_AUTO_TEST_SUITE_END();
