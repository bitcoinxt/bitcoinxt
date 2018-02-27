// Copyright (c) 2016 The Bitcoin XT developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#include <boost/test/unit_test.hpp>

#include "test/thinblockutil.h"
#include "compactblockprocessor.h"
#include "blockheaderprocessor.h"
#include "streams.h"
#include "txmempool.h"
#include "compactthin.h"
#include "chainparams.h"
#include "consensus/consensus.h"
#include "chain.h"

struct CmpctDummyHeaderProcessor : public BlockHeaderProcessor {

    CmpctDummyHeaderProcessor() : reqConnHeadResp(false) { }

    CBlockIndex* operator()(const std::vector<CBlockHeader>&, bool, bool) override {
        static CBlockIndex index;
        index.nHeight = 2;
        return &index;
    }
    bool requestConnectHeaders(const CBlockHeader& h, CNode& from, bool) override {
        return reqConnHeadResp;
    }
    bool reqConnHeadResp;
};

struct CBProcessorFixture {

    CBProcessorFixture() :
        thinmg(GetDummyThinBlockMg()),
        mpool(CFeeRate(0))
    {
        // test assert when pushing ping to pfrom if consensus params
        // are not set.
        SelectParams(CBaseChainParams::MAIN);

        // asserts if fPrintToDebugLog is true
        fPrintToDebugLog = false;
    }

    CDataStream toStream(const CompactBlock& b) {
        CDataStream blockstream(SER_NETWORK, PROTOCOL_VERSION);
        blockstream << b;
        return blockstream;
    }

    std::unique_ptr<ThinBlockManager> thinmg;
    CmpctDummyHeaderProcessor headerp;
    CTxMemPool mpool;
};

BOOST_FIXTURE_TEST_SUITE(compactblockprocessor_tests, CBProcessorFixture);

BOOST_AUTO_TEST_CASE(accepts_parallel_compacts) {

    // We may receive a block announcement
    // as a compact block while we've already
    // requested a different block as compact.
    // We need to be willing to receive both.

    DummyNode node(42, thinmg.get());
    CompactWorker w(*thinmg, node.id);

    // start work on first block.
    uint256 block1 = uint256S("0xf00");
    w.addWork(block1);

    CompactBlockProcessor p(node, w, headerp);

    CBlock block2 = TestBlock1();
    CDataStream blockstream = toStream(
            CompactBlock(block2, CoinbaseOnlyPrefiller{}));

    // cblock2 is announced, should start work on
    // that too.
    p(blockstream, mpool, MAX_BLOCK_SIZE, 1);

    BOOST_CHECK(w.isWorkingOn(block1));
    BOOST_CHECK(w.isWorkingOn(block2.GetHash()));
    BOOST_CHECK_EQUAL(1, thinmg->numWorkers(block1));
    BOOST_CHECK_EQUAL(1, thinmg->numWorkers(block2.GetHash()));
    w.stopAllWork();
    thinmg.reset();
}

BOOST_AUTO_TEST_CASE(two_process_same) {
    // Check that two nodes can process the same block at the same time,
    // even though it's two compact blocks with different idks

    CBlock block = TestBlock1();

    DummyNode node1(12, thinmg.get());
    DummyNode node2(21, thinmg.get());

    CompactWorker w1(*thinmg, node1.id);
    CompactWorker w2(*thinmg, node2.id);

    CompactBlock c1(block, CoinbaseOnlyPrefiller{});
    CompactBlock c2(block, CoinbaseOnlyPrefiller{}); // Has differet idks

    CompactBlockProcessor p1(node1, w1, headerp);
    CompactBlockProcessor p2(node2, w2, headerp);

    CDataStream s1 = toStream(c1);
    CDataStream s2 = toStream(c2);
    p1(s1, mpool, MAX_BLOCK_SIZE, 1);
    p2(s2, mpool, MAX_BLOCK_SIZE, 1);

    // both should be working on the block still
    BOOST_CHECK(w1.isWorkingOn(block.GetHash()));
    BOOST_CHECK(w2.isWorkingOn(block.GetHash()));

    // both should have sent a transaction re-request
    auto has_msg = [](DummyNode& n, const std::string& msg) {
        return std::find(begin(n.messages), end(n.messages), msg)
            != end(n.messages);
    };
    BOOST_CHECK(has_msg(node1, "getblocktxn"));
    BOOST_CHECK(has_msg(node2, "getblocktxn"));
};

BOOST_AUTO_TEST_CASE(discard_if_missing_prev) {
    // discard block (stop working on it) if we don't
    // have header for the previous block.

    CBlock block = TestBlock1();

    DummyNode node(11, thinmg.get());
    CompactWorker w(*thinmg, node.id);
    CompactBlock c(block, CoinbaseOnlyPrefiller{});
    CDataStream s = toStream(c);

    headerp.reqConnHeadResp = true; // we need to request prev header (we don't have it)

    CompactBlockProcessor p(node, w, headerp);
    p(s, mpool, MAX_BLOCK_SIZE, 1);

    // should have discarded block.
    BOOST_CHECK(!w.isWorkingOn(block.GetHash()));
};

BOOST_AUTO_TEST_SUITE_END();
