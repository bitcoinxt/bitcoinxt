// Copyright (c) 2016- The Bitcoin XT developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#include <boost/test/unit_test.hpp>
#include "blockheaderprocessor.h"
#include "test/dummyconnman.h"
#include "test/testutil.h"
#include "test/thinblockutil.h"

BOOST_AUTO_TEST_SUITE(blockheaderprocessor_tests);

BOOST_AUTO_TEST_CASE(test_connect_chain_req) {
    // Header does not connect, should
    // send a getheaders to connect and stop processing headers.

    DummyConnman connman;
    DummyNode from;

    // Try to process a header that does not connect
    DummyBlockIndexEntry e1(uint256S("0xaaa"));
    DummyBlockIndexEntry e2(uint256S("0xbbb"));

    CBlockHeader header;
    header.hashPrevBlock = uint256S("0xccc"); //< prev block not in mapBlockIndex

    auto thinmg = GetDummyThinBlockMg();
    InFlightIndex inFlight;
    DummyMarkAsInFlight markAsInFlight;
    auto checkBlockIndex = []() { };

    DefaultHeaderProcessor p(connman, &from, inFlight, *thinmg, markAsInFlight, checkBlockIndex);

    BOOST_CHECK(p.requestConnectHeaders(header, connman, from, true));
    BOOST_CHECK_EQUAL(1, NodeStatePtr(from.id)->unconnectingHeaders);
    BOOST_CHECK_EQUAL(size_t(1), connman.NumMessagesSent(from));
    BOOST_CHECK(connman.MsgWasSent(from, "getheaders", 0));

    // Test that unconnecting headers isn't bumped when requested not to
    BOOST_CHECK(p.requestConnectHeaders(header, connman, from, false));
    BOOST_CHECK_EQUAL(1 /* no change */, NodeStatePtr(from.id)->unconnectingHeaders);

    // Add entry so that header connects. Try again.

    DummyBlockIndexEntry e3(header.hashPrevBlock);

    connman.ClearMessages(from);
    BOOST_CHECK(!p.requestConnectHeaders(header, connman, from, true));
    BOOST_CHECK_EQUAL(size_t(0), connman.NumMessagesSent(from));
    BOOST_CHECK_EQUAL(1 /* no change */, NodeStatePtr(from.id)->unconnectingHeaders);
}

namespace {
    class DummyHeaderProcessor : public DefaultHeaderProcessor {
        public:
        DummyHeaderProcessor(std::unique_ptr<ThinBlockManager> mg) :
            DefaultHeaderProcessor(connman, &node, inFlight, *mg, markAsInFlight, [](){}),
            thinmg(std::move(mg))
        {
        }

        std::vector<CBlockIndex*> findMissingBlocks(CBlockIndex* last) override {
            return DefaultHeaderProcessor::findMissingBlocks(last);
        }
        private:
            DummyConnman connman;
            DummyNode node;
            InFlightIndex inFlight;
            DummyMarkAsInFlight markAsInFlight;
            std::unique_ptr<ThinBlockManager> thinmg;
    };

}

BOOST_AUTO_TEST_CASE(test_find_missing_blocks) {

    uint256 dummyhash = uint256S("0xF00D");

    CBlockIndex prev1;
    prev1.nStatus = BLOCK_HAVE_DATA;
    prev1.phashBlock = &dummyhash;

    CBlockIndex tip;
    tip.nStatus = 0;
    tip.pprev = &prev1;
    tip.phashBlock = &dummyhash;

    DummyHeaderProcessor p(GetDummyThinBlockMg());
    std::vector<CBlockIndex*> missing = p.findMissingBlocks(&tip);

    // we have data for prev1, so only tip should be returned.
    BOOST_CHECK_EQUAL(size_t(1), missing.size());
    BOOST_CHECK_EQUAL(&tip, missing[0]);

    // we don't have data for prev1, both should be returned.
    prev1.nStatus = 0;
    missing = p.findMissingBlocks(&tip);
    BOOST_CHECK_EQUAL(size_t(2), missing.size());
    BOOST_CHECK_EQUAL(&tip, missing[0]);
    BOOST_CHECK_EQUAL(&prev1, missing[1]);
}

BOOST_AUTO_TEST_CASE(test_find_missing_blocks_trim) {
    // Function should not return more than MAX_BLOCKS_IN_TRANSIT_PER_PEER,
    // skipping the newest blocks.
    uint256 dummyhash = uint256S("0xF00D");

    CBlockIndex* prev = nullptr;
    std::vector<CBlockIndex> blocks;
    blocks.resize(1 + MAX_BLOCKS_IN_TRANSIT_PER_PEER);
    for (size_t i = 0; i < blocks.size(); ++i) {
        CBlockIndex newTip;
        newTip.nStatus = 0;
        newTip.pprev = prev;
        newTip.phashBlock = &dummyhash;
        blocks[i] = newTip;
        prev = &blocks[i];
    }

    DummyHeaderProcessor p(GetDummyThinBlockMg());
    std::vector<CBlockIndex*> missing = p.findMissingBlocks(&blocks.back());

    // blocks.back() should be trimmed off.
    BOOST_CHECK_EQUAL(static_cast<size_t>(MAX_BLOCKS_IN_TRANSIT_PER_PEER), missing.size());
    BOOST_CHECK_EQUAL(&blocks[blocks.size() - 2], missing.front());
    BOOST_CHECK_EQUAL(&blocks.front(), missing.back());
}

BOOST_AUTO_TEST_CASE(test_find_missing_blocks_toofarbehind) {

    const int WALK_LIMIT = 144; // one day
    uint256 dummyhash = uint256S("0xF00D");
    CBlockIndex* prev = nullptr;
    std::vector<CBlockIndex> blocks;
    blocks.resize(1 + WALK_LIMIT);
    for (size_t i = 0; i < blocks.size(); ++i) {
        CBlockIndex newTip;
        newTip.nStatus = 0;
        newTip.pprev = prev;
        newTip.phashBlock = &dummyhash;
        blocks[i] = newTip;
        prev = &blocks[i];
    }
    DummyHeaderProcessor p(GetDummyThinBlockMg());
    std::vector<CBlockIndex*> missing = p.findMissingBlocks(&blocks.back());

    BOOST_CHECK_EQUAL(size_t(0), missing.size());
}

BOOST_AUTO_TEST_SUITE_END();
