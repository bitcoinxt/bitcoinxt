// Copyright (c) 2016 The Bitcoin XT developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#include <boost/test/unit_test_suite.hpp>
#include <boost/test/test_tools.hpp>
#include "compactprefiller.h"
#include "test/thinblockutil.h"

BOOST_AUTO_TEST_SUITE(comapactprefiller_tests)

BOOST_AUTO_TEST_CASE(coinbase_only_test) {
    CoinbaseOnlyPrefiller filler;
    CBlock block = TestBlock2();

    std::vector<PrefilledTransaction> prefilled = filler.fillFrom(block);
    BOOST_CHECK_EQUAL(1, prefilled.size());
    BOOST_CHECK_EQUAL(
            block.vtx.at(0).GetHash().ToString(),
            prefilled.at(0).tx.GetHash().ToString());
}

BOOST_AUTO_TEST_CASE(inventory_known_test) {

    CBlock block = TestBlock1();

    // Transactions that we know that peer knows about.
    uint256 known1 = block.vtx[2].GetHash();
    uint256 known2 = block.vtx[3].GetHash();
    uint256 known3 = block.vtx[5].GetHash();

    std::unique_ptr<CRollingBloomFilter> inventoryKnown(new CRollingBloomFilter(100, 0.000001));

    inventoryKnown->insert(known1);
    inventoryKnown->insert(known2);
    inventoryKnown->insert(known3);

    InventoryKnownPrefiller filler(std::move(inventoryKnown));
    std::vector<PrefilledTransaction> prefilled = filler.fillFrom(block);

    // Should have prefilled all except known1, known2 and known3
    BOOST_CHECK_EQUAL(block.vtx.size() - 3, prefilled.size());

    // inventoryKnown should not be included
    auto findFunc = [&known1, &known2, &known3](const PrefilledTransaction& tx) {
        return tx.tx.GetHash() == known1
            || tx.tx.GetHash() == known2
            || tx.tx.GetHash() == known3;
    };
    auto res = std::find_if(
            begin(prefilled), end(prefilled), findFunc);
    BOOST_CHECK(res == end(prefilled));

    // indexes should be "differentially encoded"
    BOOST_CHECK_EQUAL(prefilled.at(0).index, 0);
    BOOST_CHECK_EQUAL(prefilled.at(1).index, 0);
    BOOST_CHECK_EQUAL(prefilled.at(2).index, 2);
    BOOST_CHECK_EQUAL(prefilled.at(3).index, 1);
}

unsigned int sumTx(std::vector<CTransaction>& txs) {
    unsigned int sum = 0;
    for (auto& t : txs)
        sum += ::GetSerializeSize(t, SER_NETWORK, PROTOCOL_VERSION);
    return sum;
}

unsigned int sumTx(std::vector<PrefilledTransaction>& txs) {
    unsigned int sum = 0;
    for (auto& t : txs)
        sum += ::GetSerializeSize(t.tx, SER_NETWORK, PROTOCOL_VERSION);
    return sum;
}

BOOST_AUTO_TEST_CASE(prefill_dont_exceed_10kb) {
    // From BIP152
    // Nodes sending cmpctblock messages SHOULD limit prefilledtxn to 10KB of transactions

    CBlock block = TestBlock1();

    // Block with ~20KB tx
    while (sumTx(block.vtx) < 20 * 1000 /* 20 KB */) {
        CMutableTransaction newTx(block.vtx.back());
        newTx.vin[0].prevout.hash = GetRandHash();
        block.vtx.push_back(CTransaction(newTx));
    }

    // Filter matches nothing, should prefill as much as possible.
    std::unique_ptr<CRollingBloomFilter> inventoryKnown(new CRollingBloomFilter(100, 0.000001));
    InventoryKnownPrefiller filler(std::move(inventoryKnown));

    std::vector<PrefilledTransaction> prefilled = filler.fillFrom(block);

    BOOST_CHECK(sumTx(prefilled) < 10 * 1000 /* 10KB */);
    BOOST_CHECK(sumTx(prefilled) > 9 * 1000); // Want close to the limit
}

BOOST_AUTO_TEST_CASE(test_choose_prefiller) {

    DummyNode node;
    node.fRelayTxes = false;

    std::unique_ptr<CompactPrefiller> prefiller = choosePrefiller(node);
    BOOST_CHECK(dynamic_cast<CoinbaseOnlyPrefiller*>(prefiller.get()) != nullptr);

    node.fRelayTxes = true;
    prefiller = choosePrefiller(node);
    BOOST_CHECK(dynamic_cast<InventoryKnownPrefiller*>(prefiller.get()) != nullptr);
}

BOOST_AUTO_TEST_SUITE_END()
