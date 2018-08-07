// Copyright (c) 2018 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "net.h" // for CConnman
#include "random.h"
#include "respend/respendrelayer.h"
#include "test/test_bitcoin.h"
#include "test/thinblockutil.h"

#include <boost/test/unit_test.hpp>

using namespace respend;

BOOST_FIXTURE_TEST_SUITE(respendrelayer_tests, BasicTestingSetup);

BOOST_AUTO_TEST_CASE(not_interesting) {
    CConnman connman(0, 0);
    RespendRelayer r(&connman);
    BOOST_CHECK(!r.IsInteresting());
    CTxMemPool::txiter dummy;
    bool lookAtMore;

    lookAtMore = r.AddOutpointConflict(COutPoint{}, dummy, CTransaction{},
                                            true /* seen before */, false);
    BOOST_CHECK(lookAtMore);
    BOOST_CHECK(!r.IsInteresting());

    lookAtMore = r.AddOutpointConflict(COutPoint{}, dummy, CTransaction{},
                                       false, true /* is equivalent */);
    BOOST_CHECK(lookAtMore);
    BOOST_CHECK(!r.IsInteresting());
}

BOOST_AUTO_TEST_CASE(is_interesting) {
    CConnman connman(0, 0);
    RespendRelayer r(&connman);
    CTxMemPool::txiter dummy;
    bool lookAtMore;

    lookAtMore = r.AddOutpointConflict(COutPoint{}, dummy, CTransaction{}, false, false);
    BOOST_CHECK(!lookAtMore);
    BOOST_CHECK(r.IsInteresting());
}

BOOST_AUTO_TEST_CASE(triggers_correctly) {
    CTxMemPool::txiter dummy;
    CMutableTransaction respend;
    respend.vin.resize(1);
    respend.vin[0].prevout.n = 0;
    respend.vin[0].prevout.hash = GetRandHash();
    respend.vin[0].scriptSig << OP_1;

    DummyNode node;
    node.fRelayTxes = true;
    CConnman connman(0, 0);
    connman.AddTestNode(&node);

    // Create a "not interesting" respend
    RespendRelayer r(&connman);
    r.AddOutpointConflict(COutPoint{}, dummy, respend, true, false);
    r.Trigger();
    BOOST_CHECK_EQUAL(size_t(0), node.vInventoryToSend.size());
    r.SetValid(true);
    r.Trigger();
    BOOST_CHECK_EQUAL(size_t(0), node.vInventoryToSend.size());

    // Create an interesting, but invalid respend
    r.AddOutpointConflict(COutPoint{}, dummy, respend, false, false);
    BOOST_CHECK(r.IsInteresting());
    r.SetValid(false);
    r.Trigger();
    BOOST_CHECK_EQUAL(size_t(0), node.vInventoryToSend.size());
    // make valid
    r.SetValid(true);
    r.Trigger();
    BOOST_CHECK_EQUAL(size_t(1), node.vInventoryToSend.size());
    BOOST_CHECK(respend.GetHash() == node.vInventoryToSend.at(0).hash);

    // Create an interesting and valid respend to an SPV peer.
    //
    // As a precation, we *don't* relay respends to SPV nodes. They may not be
    // tracking the original transaction.
    node.pfilter.reset(new CBloomFilter(1, .00001, 5, BLOOM_UPDATE_ALL));
    node.pfilter->insert(respend.GetHash());
    node.vInventoryToSend.clear();
    r.SetValid(true);
    r.Trigger();
    BOOST_CHECK_EQUAL(size_t(0), node.vInventoryToSend.size());
    node.pfilter->clear();

    connman.RemoveTestNode(&node);
}

BOOST_AUTO_TEST_SUITE_END();
