// Copyright (c) 2016- The Bitcoin XT developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <boost/test/unit_test.hpp>
#include "test/testutil.h"
#include "blockannounce.h"
#include "inflightindex.h"
#include "test/thinblockutil.h"
#include "options.h"
#include "xthin.h"
#include "dummythin.h"

struct DummyBlockAnnounceProcessor : public BlockAnnounceProcessor {

    DummyBlockAnnounceProcessor(uint256 block,
            CNode& from, ThinBlockManager& thinmg, InFlightIndex& inFlightIndex) : 
        BlockAnnounceProcessor(block, from, thinmg, inFlightIndex),
        updateCalled(0), 
        isAlmostSynced(true),
        overrideStrategy(BlockAnnounceProcessor::INVALID)
    {
    }
    
    void updateBlockAvailability() override {
        ++updateCalled;
    }
        
    bool almostSynced() override {
        return isAlmostSynced;
    }

    DownloadStrategy pickDownloadStrategy() override {
        return overrideStrategy != INVALID
            ? overrideStrategy
            : BlockAnnounceProcessor::pickDownloadStrategy();
    }
        
    bool wantBlock() const override {
        return BlockAnnounceProcessor::wantBlock();
    }
        
    bool isInitialBlockDownload() const override {
        return false;
    }

    int updateCalled;
    bool isAlmostSynced;
    DownloadStrategy overrideStrategy;
};

struct DummyInFlightIndex : public InFlightIndex {
    DummyInFlightIndex() : InFlightIndex(), blockInFlight(false) { }
    virtual bool isInFlight(const uint256& block) const override {
        return blockInFlight;
    }
    bool blockInFlight;

};

struct BlockAnnounceFixture {
    BlockAnnounceFixture() : 
        block(uint256S("0xF00BAA")),
        thinmg(GetDummyThinBlockMg()),
        node(42, thinmg.get()),
        ann(block, node, *thinmg, inFlightIndex),
        nodestate(node.id)
    {
        SelectParams(CBaseChainParams::MAIN);
        nodestate->initialHeadersReceived = true;
    }

    uint256 block;
    std::unique_ptr<ThinBlockManager> thinmg;
    DummyNode node;
    DummyInFlightIndex inFlightIndex;
    DummyBlockAnnounceProcessor ann;
    NodeStatePtr nodestate;
};

BOOST_FIXTURE_TEST_SUITE(blockannounce_tests, BlockAnnounceFixture);

BOOST_AUTO_TEST_CASE(want_block) {
    
    // We have not seen the block before. We want it.
    BOOST_CHECK(ann.wantBlock());

    // We have block, we don't want it.
    DummyBlockIndexEntry entry(block);
    BOOST_CHECK(!ann.wantBlock());
}

BOOST_AUTO_TEST_CASE(announce_updates_availability) {

    std::vector<CInv> toFetch;
    
    ann.onBlockAnnounced(toFetch);
    BOOST_CHECK_EQUAL(1, ann.updateCalled);

    // Availability should also be called when we don't want block.
    DummyBlockIndexEntry entry(block); //< we have block (we don't want it)
    ann.onBlockAnnounced(toFetch);
    BOOST_CHECK_EQUAL(2, ann.updateCalled);
}

BOOST_AUTO_TEST_CASE(fetch_when_wanted) {

    {   // We want block.
        std::vector<CInv> toFetch;
        BOOST_CHECK(ann.onBlockAnnounced(toFetch));
        BOOST_CHECK(!toFetch.empty());
    }

    {   // We have block (we don't want it)
        std::vector<CInv> toFetch;
        DummyBlockIndexEntry entry(block); 
        BOOST_CHECK(!ann.onBlockAnnounced(toFetch));
        BOOST_CHECK_EQUAL(0, toFetch.size());
    }
}

const int NOT_SET = std::numeric_limits<int>::min();

struct DummyArgGetter : public ArgGetter {

    DummyArgGetter() : ArgGetter(),
        usethin(NOT_SET), maxparallel(NOT_SET)
    {
    }

    virtual int64_t GetArg(const std::string& arg, int64_t def) {
        if (arg == "-use-thin-blocks")
            return usethin == NOT_SET ? def : usethin;
        if (arg == "-thin-blocks-max-parallel")
            return maxparallel == NOT_SET ? def : maxparallel;
        return def;
    }
    virtual std::vector<std::string> GetMultiArgs(const std::string& arg) {
        assert(false);
    }
    virtual bool GetBool(const std::string& arg, bool def) {
        if (arg == "-use-thin-blocks")
            return usethin == NOT_SET ? def : bool(usethin);
        return def;
    }
    int usethin;
    int maxparallel;
};

BOOST_AUTO_TEST_CASE(dowl_strategy_full_now) {
    
    // Peer does not support thin blocks and we are almost synced.
    node.nServices = 0;

    BOOST_CHECK_EQUAL(
            BlockAnnounceProcessor::DOWNL_FULL_NOW, 
            ann.pickDownloadStrategy());

    // Node supports thin, but thin is disabled.
    std::unique_ptr<DummyArgGetter> arg(new DummyArgGetter);
    DummyArgGetter* argPtr = arg.get();
    std::unique_ptr<ArgReset> argraii
        = SetDummyArgGetter(std::unique_ptr<ArgGetter>(arg.release()));

    node.nServices = NODE_THIN;
    argPtr->usethin = 0;
    BOOST_CHECK_EQUAL(
            BlockAnnounceProcessor::DOWNL_FULL_NOW, 
            ann.pickDownloadStrategy());
}

BOOST_AUTO_TEST_CASE(downl_strategy_thin_later) {

    // Node supports thin blocks, but we have not
    // received any headers from node yet. We may not have
    // caught up on its longest chain. Download thin later.
    node.nServices = NODE_THIN;
    nodestate->initialHeadersReceived = false;
    BOOST_CHECK_EQUAL(
            BlockAnnounceProcessor::DOWNL_THIN_LATER,
            ann.pickDownloadStrategy());
}


BOOST_AUTO_TEST_CASE(dowl_strategy_thin_later_2) {

    // Thin supports sending thin blocks but is busy. Request later.
    node.nServices = NODE_THIN;

    // By default DummyNode uses DummyThinWorker. 
    // DummyThinWorker always returns false for isAvailable().
    BOOST_ASSERT(!nodestate->thinblock->isAvailable());


    BOOST_CHECK_EQUAL(
            BlockAnnounceProcessor::DOWNL_THIN_LATER,
            ann.pickDownloadStrategy());
}

BOOST_AUTO_TEST_CASE(dowl_strategy_dont_downl_1) {

    // If we have requested block from max number of nodes
    // then don't download.
    
    // Set max parallel to 1
    auto arg(new DummyArgGetter);
    std::unique_ptr<ArgReset> argraii
        = SetDummyArgGetter(std::unique_ptr<ArgGetter>(arg));
    arg->maxparallel = 1;
    BOOST_CHECK_EQUAL(1, Opt().ThinBlocksMaxParallel());

    // Put one node to work
    DummyNode node2(11, thinmg.get());
    NodeStatePtr state2(node2.id);
    state2->initialHeadersReceived = true;
    state2->thinblock.reset(new XThinWorker(*thinmg, node2.id));
    state2->thinblock->setToWork(block);
    BOOST_CHECK_EQUAL(1, thinmg->numWorkers(block));

    // Second one should not attempt to download on announcement.
    nodestate->thinblock.reset(new XThinWorker(*thinmg, node.id));
    node.nServices = NODE_THIN;
    
    BOOST_CHECK_EQUAL(BlockAnnounceProcessor::DONT_DOWNL,
            ann.pickDownloadStrategy());
}

BOOST_AUTO_TEST_CASE(dowl_strategy_dont_downl_2) {
    // Don't download announced block unless we are close to
    // catching up to the longest chain.
    
    ann.isAlmostSynced = false;
    BOOST_CHECK_EQUAL(BlockAnnounceProcessor::DONT_DOWNL, ann.pickDownloadStrategy());
}

BOOST_AUTO_TEST_CASE(dowl_strategy_dont_dowl_3) {
    // Don't download a block that is already in flight.
    
    node.nServices &= ~NODE_THIN;

    inFlightIndex.blockInFlight = true;
    BOOST_CHECK_EQUAL(BlockAnnounceProcessor::DONT_DOWNL,
            ann.pickDownloadStrategy());
}

BOOST_AUTO_TEST_CASE(dowl_strategy_dont_dowl_4) {
    
    // Don't download a block from a peer that has
    // many blocks queued up.
    
    nodestate->nBlocksInFlight = MAX_BLOCKS_IN_TRANSIT_PER_PEER;
    BOOST_CHECK_EQUAL(BlockAnnounceProcessor::DONT_DOWNL,
            ann.pickDownloadStrategy());
}

BOOST_AUTO_TEST_CASE(dowl_strategy_dont_dowl_5) {
    
    // Don't download a full block from a peer if we
    // are configured to avoid full block downloads.
    
    node.nServices &= ~NODE_THIN;
    
    auto arg = new DummyArgGetter;
    auto argraii = SetDummyArgGetter(std::unique_ptr<ArgGetter>(arg));
    arg->usethin = 3; // xthin only, avoid full blocks

    BOOST_CHECK_EQUAL(BlockAnnounceProcessor::DONT_DOWNL,
            ann.pickDownloadStrategy());
}

/// Helper class to check if request block is called.
struct RequestBlockWorker : public DummyThinWorker {
    RequestBlockWorker(ThinBlockManager& mg, NodeId id)
        : DummyThinWorker(mg, id), reqs(0) { }
    
    void requestBlock(const uint256& block,
        std::vector<CInv>& getDataReq, CNode& node) override { 
        ++reqs;
    }
    int reqs;
};


BOOST_AUTO_TEST_CASE(onannounced_downl_thin) {
    // Test that onBlockAnnounced requests a thin block
    // with DOWL_THIN_NOW strategy.

    ann.overrideStrategy = BlockAnnounceProcessor::DOWNL_THIN_NOW;

    auto worker = new RequestBlockWorker(*thinmg, node.id);
    nodestate->thinblock.reset(worker); //<- takes ownership of ptr
    
    std::vector<CInv> r;
    BOOST_CHECK(ann.onBlockAnnounced(r));
    BOOST_CHECK_EQUAL(1, worker->reqs);

    // thin blocks come with a header, we should not
    // have requested headers.
    BOOST_CHECK_EQUAL(0, node.messages.size());
}

BOOST_AUTO_TEST_CASE(onannounced_downl_full) {
    // Test that onBlockAnnounced requests a full block
    // with DOWL_FULL_NOW strategy.
    
    ann.overrideStrategy = BlockAnnounceProcessor::DOWNL_FULL_NOW;
    
    std::vector<CInv> toFetch;
    BOOST_CHECK(ann.onBlockAnnounced(toFetch));
    BOOST_CHECK_EQUAL(1, toFetch.size());
    BOOST_CHECK_EQUAL(block.ToString(), toFetch.at(0).hash.ToString());

    // Should also have requested headers.
    BOOST_CHECK_EQUAL("getheaders", node.messages.at(0));
}

BOOST_AUTO_TEST_CASE(onannounced_dont_downl) {
    // Test that onBlockAnnounced does not request anything 
    // with DONT_DOWNL strategy.

    ann.overrideStrategy = BlockAnnounceProcessor::DONT_DOWNL;

    auto worker = new RequestBlockWorker(*thinmg, node.id);
    nodestate->thinblock.reset(worker); //<- takes ownership of ptr
    
    std::vector<CInv> toFetch;
    BOOST_CHECK(!ann.onBlockAnnounced(toFetch));
    BOOST_CHECK_EQUAL(0, worker->reqs);
    BOOST_CHECK_EQUAL(0, toFetch.size());

    // If we don't download, we should still ask for the header.
    BOOST_ASSERT(node.messages.size() > 0);
    BOOST_CHECK_EQUAL("getheaders", node.messages.at(0));
}

BOOST_AUTO_TEST_CASE(onannounced_dowl_thin_later) {
    // Test that onBlockAnnounced does not request anything 
    // with DOWL_THIN_LATER strategy.

    ann.overrideStrategy = BlockAnnounceProcessor::DOWNL_THIN_LATER;

    auto worker = new RequestBlockWorker(*thinmg, node.id);
    nodestate->thinblock.reset(worker); //<- takes ownership of ptr
    
    std::vector<CInv> toFetch;
    BOOST_CHECK(!ann.onBlockAnnounced(toFetch));
    BOOST_CHECK_EQUAL(0, worker->reqs);
    BOOST_CHECK_EQUAL(0, toFetch.size());

    // If we don't download, we should still ask for the header.
    BOOST_ASSERT(node.messages.size() > 0);
    BOOST_CHECK_EQUAL("getheaders", node.messages.at(0));
}

BOOST_AUTO_TEST_SUITE_END();
