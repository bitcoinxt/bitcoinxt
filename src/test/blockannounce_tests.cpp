// Copyright (c) 2016-2017 The Bitcoin XT developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <boost/test/unit_test.hpp>
#include "test/testutil.h"
#include "blockannounce.h"
#include "blocksender.h"
#include "inflightindex.h"
#include "test/thinblockutil.h"
#include "options.h"
#include "xthin.h"
#include "dummythin.h"

struct DummyBlockAnnounceReceiver : public BlockAnnounceReceiver {

    DummyBlockAnnounceReceiver(uint256 block,
            CNode& from, ThinBlockManager& thinmg, InFlightIndex& inFlightIndex) :
        BlockAnnounceReceiver(block, from, thinmg, inFlightIndex),
        updateCalled(0),
        isAlmostSynced(true),
        overrideStrategy(BlockAnnounceReceiver::INVALID)
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
            : BlockAnnounceReceiver::pickDownloadStrategy();
    }

    bool blockHeaderIsKnown() const override {
        return BlockAnnounceReceiver::blockHeaderIsKnown();
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

struct BlockAnnounceRecvFixture {

    BlockAnnounceRecvFixture() :
        block(uint256S("0xF00BAA")),
        thinmg(GetDummyThinBlockMg()),
        node(42, thinmg.get()),
        ann(block, node, *thinmg, inFlightIndex),
        nodestate(node.id)
    {
        SelectParams(CBaseChainParams::MAIN);
    }

    uint256 block;
    std::unique_ptr<ThinBlockManager> thinmg;
    DummyNode node;
    DummyInFlightIndex inFlightIndex;
    DummyBlockAnnounceReceiver ann;
    NodeStatePtr nodestate;
};

BOOST_FIXTURE_TEST_SUITE(blockannouncerecv_tests, BlockAnnounceRecvFixture);

BOOST_AUTO_TEST_CASE(header_is_known) {

    // We have not seen the block before.
    BOOST_CHECK(!ann.blockHeaderIsKnown());

    // We have an entry, header is known.
    DummyBlockIndexEntry entry(block);
    BOOST_CHECK(ann.blockHeaderIsKnown());
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
            BlockAnnounceReceiver::DOWNL_FULL_NOW,
            ann.pickDownloadStrategy());

    // Node supports thin, but thin is disabled.
    std::unique_ptr<DummyArgGetter> arg(new DummyArgGetter);
    DummyArgGetter* argPtr = arg.get();
    std::unique_ptr<ArgReset> argraii
        = SetDummyArgGetter(std::unique_ptr<ArgGetter>(arg.release()));

    node.nServices = NODE_THIN;
    argPtr->usethin = 0;
    BOOST_CHECK_EQUAL(
            BlockAnnounceReceiver::DOWNL_FULL_NOW,
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
    state2->thinblock.reset(new XThinWorker(*thinmg, node2.id));
    state2->thinblock->addWork(block);
    BOOST_CHECK_EQUAL(1, thinmg->numWorkers(block));

    // Second one should not attempt to download on announcement.
    nodestate->thinblock.reset(new XThinWorker(*thinmg, node.id));
    node.nServices = NODE_THIN;

    BOOST_CHECK_EQUAL(BlockAnnounceReceiver::DONT_DOWNL,
            ann.pickDownloadStrategy());
}

BOOST_AUTO_TEST_CASE(dowl_strategy_dont_downl_2) {
    // Don't download announced block unless we are close to
    // catching up to the longest chain.

    ann.isAlmostSynced = false;
    BOOST_CHECK_EQUAL(BlockAnnounceReceiver::DONT_DOWNL, ann.pickDownloadStrategy());
}

BOOST_AUTO_TEST_CASE(dowl_strategy_dont_dowl_3) {
    // Don't download a block that is already in flight.

    node.nServices &= ~NODE_THIN;

    inFlightIndex.blockInFlight = true;
    BOOST_CHECK_EQUAL(BlockAnnounceReceiver::DONT_DOWNL,
            ann.pickDownloadStrategy());
}

BOOST_AUTO_TEST_CASE(dowl_strategy_dont_dowl_4) {

    // Don't download a block from a peer that has
    // many blocks queued up.

    nodestate->nBlocksInFlight = MAX_BLOCKS_IN_TRANSIT_PER_PEER;
    BOOST_CHECK_EQUAL(BlockAnnounceReceiver::DONT_DOWNL,
            ann.pickDownloadStrategy());
}

BOOST_AUTO_TEST_CASE(dowl_strategy_dont_dowl_5) {

    // Don't download a full block from a peer if we
    // are configured to avoid full block downloads.

    node.nServices &= ~NODE_THIN;

    auto arg = new DummyArgGetter;
    auto argraii = SetDummyArgGetter(std::unique_ptr<ArgGetter>(arg));
    arg->usethin = 3; // xthin only, avoid full blocks

    BOOST_CHECK_EQUAL(BlockAnnounceReceiver::DONT_DOWNL,
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

    ann.overrideStrategy = BlockAnnounceReceiver::DOWNL_THIN_NOW;

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

    ann.overrideStrategy = BlockAnnounceReceiver::DOWNL_FULL_NOW;

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

    ann.overrideStrategy = BlockAnnounceReceiver::DONT_DOWNL;

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

    ann.overrideStrategy = BlockAnnounceReceiver::DOWNL_THIN_LATER;

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

// Inherit to test protected methods
class DummyBlockAnnounceSender : BlockAnnounceSender {
    public:
        DummyBlockAnnounceSender(CNode& to) : BlockAnnounceSender(to) { }
        bool canAnnounceWithHeaders() const override {
            return BlockAnnounceSender::canAnnounceWithHeaders();
        }
        bool canAnnounceWithBlock() const override {
            return BlockAnnounceSender::canAnnounceWithBlock();
        }
        bool announceWithHeaders() override {
            return BlockAnnounceSender::announceWithHeaders();
        }
        void announceWithInv() override {
            return BlockAnnounceSender::announceWithInv();
        }
        void announceWithBlock(BlockSender& s) override {
            return BlockAnnounceSender::announceWithBlock(s);
        }
};

struct BlockAnnounceSenderFixture {
    BlockAnnounceSenderFixture() :
        ann(to)
    {
        SelectParams(CBaseChainParams::MAIN);
    }

    DummyNode to;
    DummyBlockAnnounceSender ann;
};

struct SetTipRAII {
    SetTipRAII(CBlockIndex* newTip) {
        chainActive.SetTip(newTip);
    }
    ~SetTipRAII() { chainActive.SetTip(nullptr); }
};

BOOST_FIXTURE_TEST_SUITE(blockannouncesend_tests, BlockAnnounceSenderFixture);

BOOST_AUTO_TEST_CASE(canAnnounceWithHeaders) {

    uint256 block = uint256S("0xABBA");
    // Add block to our active chain.
    DummyBlockIndexEntry entry(block);
    SetTipRAII activeTip(&entry.index);

    NodeStatePtr(to.id)->prefersHeaders = true;
    to.blocksToAnnounce = std::vector<uint256>(1, block);
    BOOST_CHECK(ann.canAnnounceWithHeaders());

    // To many header announcements
    to.blocksToAnnounce = std::vector<uint256>(MAX_BLOCKS_TO_ANNOUNCE + 1, block);
    BOOST_CHECK(!ann.canAnnounceWithHeaders());

    // We want to announce blocks in our active chain. Thats fine.
    to.blocksToAnnounce = std::vector<uint256>(1, block);
    BOOST_CHECK(ann.canAnnounceWithHeaders());

    // A block not in our active chain,
    // for example: we re-orged from it.
    DummyBlockIndexEntry entry2(uint256S("0xFEED"));
    to.blocksToAnnounce.push_back(entry2.hash);
    BOOST_CHECK(!ann.canAnnounceWithHeaders());
}

// Test for edge case where headers to announce
// do not connect.
BOOST_AUTO_TEST_CASE(canAnnounceWithHeaders_canconnect) {
    DummyBlockIndexEntry entry1(uint256S("0xF00"));
    DummyBlockIndexEntry entry2(uint256S("0xBAA"));
    entry1.index.nHeight = 0;
    entry2.index.nHeight = 1;

    to.blocksToAnnounce = { entry1.hash, entry2.hash };
    NodeStatePtr(to.id)->bestHeaderSent = &entry1.index;
    NodeStatePtr(to.id)->prefersHeaders = true;

    {   // blocks don't connect
        SetTipRAII activeTip(&entry2.index);
        BOOST_CHECK(!ann.canAnnounceWithHeaders());
    }

    {   // blocks do connect
        entry2.index.pprev = &entry1.index;
        SetTipRAII activeTip(&entry2.index);
        BOOST_CHECK(ann.canAnnounceWithHeaders());
    }
}

BOOST_AUTO_TEST_CASE(announce_with_headers) {
    DummyBlockIndexEntry entry1(uint256S("0xBAD"));
    DummyBlockIndexEntry entry2(uint256S("0xBEEF"));
    DummyBlockIndexEntry entry3(uint256S("0xF00D"));
    entry2.index.pprev = &entry1.index;
    entry3.index.pprev = &entry2.index;

    // Can't connect headers with known best on peer, bail out.
    to.blocksToAnnounce = { entry2.hash, entry3.hash };
    BOOST_CHECK(!ann.announceWithHeaders());
    BOOST_CHECK_EQUAL(0, to.messages.size());

    // Peer knows about entry1. Should announce entry2 and entry3.
    to.blocksToAnnounce = { entry1.hash, entry2.hash, entry3.hash };
    NodeStatePtr(to.id)->bestHeaderSent = &entry1.index;
    BOOST_CHECK(ann.announceWithHeaders());
    BOOST_CHECK_EQUAL(1, to.messages.size());
    BOOST_CHECK_EQUAL("headers", to.messages.at(0));

    // bestHeaderSent should now be entry3
    BOOST_CHECK(NodeStatePtr(to.id)->bestHeaderSent == &entry3.index);
}

BOOST_AUTO_TEST_CASE(announce_with_inv) {
    DummyBlockIndexEntry entry1(uint256S("0xBAD"));
    DummyBlockIndexEntry entry2(uint256S("0xBEEF"));
    entry2.index.pprev = &entry1.index;

    to.blocksToAnnounce = { entry1.hash, entry2.hash };
    NodeStatePtr(to.id)->bestHeaderSent = &entry1.index;

    ann.announceWithInv();

    BOOST_CHECK_EQUAL(1, to.vInventoryToSend.size());
    BOOST_CHECK_EQUAL(MSG_BLOCK, to.vInventoryToSend.at(0).type);
    BOOST_CHECK_EQUAL(
            entry2.hash.ToString(),
            to.vInventoryToSend.at(0).hash.ToString());
}

BOOST_AUTO_TEST_CASE(can_announce_with_block) {

    /// These are same criterias as for announcing with headers.
    /// All must be fulfilled, in addition to more.

    uint256 block = uint256S("0xABBA");
    // Add block to our active chain.
    DummyBlockIndexEntry entry(block);
    SetTipRAII activeTip(&entry.index);
    to.blocksToAnnounce = std::vector<uint256>(1, block);
    BOOST_CHECK(ann.canAnnounceWithBlock());

    // Can only announce with a block
    // if we have a single announcement
    to.blocksToAnnounce = std::vector<uint256>(2, block);
    BOOST_CHECK(!ann.canAnnounceWithBlock());

    // We want to announce a block in our active chain. Thats fine.
    to.blocksToAnnounce = std::vector<uint256>(1, block);
    BOOST_CHECK(ann.canAnnounceWithBlock());

    // A block not in our active chain,
    // for example: we re-orged from it.
    DummyBlockIndexEntry entry2(uint256S("0xFEED"));
    to.blocksToAnnounce = std::vector<uint256>(1, entry2.hash);
    BOOST_CHECK(!ann.canAnnounceWithBlock());
}

BOOST_AUTO_TEST_CASE(announce_with_block) {
    DummyBlockIndexEntry entry1(uint256S("0xBAD"));
    DummyBlockIndexEntry entry2(uint256S("0xBEEF"));
    entry2.index.pprev = &entry1.index;

    // Since it's only one block to announce, it
    // can be announced with block.
    to.blocksToAnnounce = { entry2.hash };
    NodeStatePtr(to.id)->bestHeaderSent = &entry1.index;
    NodeStatePtr(to.id)->supportsCompactBlocks = true;

    struct DummyBlockSender : public BlockSender {

        bool readBlockFromDisk(CBlock& block, const CBlockIndex*) override {
            block = TestBlock1();
            return true;
        }
    };
    DummyBlockSender sender;
    ann.announceWithBlock(sender);
    BOOST_CHECK_EQUAL(1, to.messages.size());
    BOOST_CHECK_EQUAL("cmpctblock", to.messages.at(0));

    // bestHeaderSent should now be entry3
    BOOST_CHECK(NodeStatePtr(to.id)->bestHeaderSent == &entry2.index);
}

BOOST_AUTO_TEST_CASE(find_headers_to_announce) {
    DummyBlockIndexEntry entry1(uint256S("0xBAD"));
    DummyBlockIndexEntry entry2(uint256S("0xBEEF"));
    DummyBlockIndexEntry entry3(uint256S("0xF00D"));
    DummyBlockIndexEntry entry4(uint256S("0xFEED"));
    entry2.index.pprev = &entry1.index;
    entry3.index.pprev = &entry2.index;
    entry4.index.pprev = &entry3.index;
    entry1.index.nHeight = 0;
    entry2.index.nHeight = 1;
    entry3.index.nHeight = 2;
    entry4.index.nHeight = 3;

    std::vector<uint256> toAnnounce = findHeadersToAnnounce(&entry2.index, &entry4.index);
    BOOST_CHECK_EQUAL(2, toAnnounce.size());
    BOOST_CHECK_EQUAL(entry3.hash.ToString(), toAnnounce.at(0).ToString());
    BOOST_CHECK_EQUAL(entry4.hash.ToString(), toAnnounce.at(1).ToString());
}

BOOST_AUTO_TEST_SUITE_END();
