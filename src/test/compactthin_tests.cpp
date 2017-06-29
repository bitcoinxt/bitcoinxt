// Copyright (c) 2016- The Bitcoin XT developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#include <boost/test/unit_test.hpp>

#include "test/thinblockutil.h"
#include "compactthin.h"
#include "chainparams.h"
#include "blockencodings.h"
#include "compactthin.h"


// Workaround for segfaulting
struct Workaround {
    Workaround() {
        SelectParams(CBaseChainParams::MAIN);
    }
};

BOOST_FIXTURE_TEST_SUITE(compactthin_tests, Workaround);

BOOST_AUTO_TEST_CASE(stub_tx_hashtypes) {
    CBlock block = TestBlock1();
    CompactBlock cmpctblock(block, CoinbaseOnlyPrefiller{});

    // CompactBlock is initialized with coinbase prefilled.
    // Prefilled transactions have full hash.
    CompactStub stub(cmpctblock);
    std::vector<ThinTx> all = stub.allTransactions();
    for (auto& t : all)
        BOOST_CHECK(!t.isNull());

    // Coinbase should have full hash.
    BOOST_CHECK(all.at(0).hasFull());
    BOOST_CHECK_EQUAL(
            block.vtx.at(0).GetHash().ToString(),
            all.at(0).full().ToString());

    // All others should only have an obfuscated "short id" (we
    // don't know full nor cheap hash)
    for (size_t i = 1; i < all.size(); ++i) {
        BOOST_CHECK(!all.at(i).hasFull());
        if (all.at(i).hasFull())
            std::cerr << all.at(i).full().ToString() << std::endl;
        BOOST_CHECK(!all.at(i).hasCheap());
        BOOST_CHECK(all.at(i).equals(block.vtx.at(i).GetHash()));
    }
}

BOOST_AUTO_TEST_CASE(ids_are_correct) {
    CBlock testblock = TestBlock1();

    std::unique_ptr<CRollingBloomFilter> inventoryKnown(new CRollingBloomFilter(100, 0.000001));
    for (size_t i = 1; i < testblock.vtx.size(); ++i) {

        if (i == 3 || i == 5)
            continue;

        inventoryKnown->insert(testblock.vtx[i].GetHash());
    }

    CompactBlock thin(testblock, InventoryKnownPrefiller(std::move(inventoryKnown)));
    CompactStub stub(thin);

    auto all = stub.allTransactions();
    BOOST_CHECK_EQUAL(testblock.vtx.size(), all.size());

    // index 0, 3 and 5 should have full id, rest shortid.
    for (size_t i = 0; i < all.size(); ++i) {
        if (i == 0 || i == 3 || i == 5) {
            BOOST_CHECK_EQUAL(
                    testblock.vtx[i].GetHash().ToString(),
                    all[i].full().ToString());
            continue;
        }
        uint64_t shortID = GetShortID(
                thin.shorttxidk0, thin.shorttxidk1,
                testblock.vtx[i].GetHash());

        BOOST_CHECK_EQUAL(shortID, all[i].obfuscated());
    }
}

BOOST_AUTO_TEST_CASE(request_block_announcements) {
    auto mg = GetDummyThinBlockMg();

    DummyNode node(42, mg.get());
    CompactWorker w(*mg, node.id);
    {
        auto h = w.requestBlockAnnouncements(node);

        // Should send a request for header announcements
        BOOST_CHECK_EQUAL(1, node.messages.size());
        BOOST_CHECK_EQUAL("sendcmpct", node.messages.at(0));
    }
    // hande goes out of scope,
    // should send a request to disable header announcements
    BOOST_CHECK_EQUAL(2, node.messages.size());
    BOOST_CHECK_EQUAL("sendcmpct", node.messages.at(1));
}

BOOST_AUTO_TEST_SUITE_END();
