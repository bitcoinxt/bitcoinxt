#include "test/thinblockutil.h"
#include "blockheaderprocessor.h"
#include "bloom.h"
#include "bloomthin.h"
#include "chain.h"
#include "chainparams.h"
#include "inflightindex.h"
#include "merkleblock.h"
#include "net.h"
#include "process_merkleblock.h"
#include "process_xthinblock.h"
#include "testutil.h"
#include "util.h" // for fPrintToDebugLog
#include "xthin.h"
#include "compactthin.h"
#include "compactblockprocessor.h"
#include <boost/test/unit_test_suite.hpp>
#include <boost/test/test_tools.hpp>

template <class WORKER_TYPE>
struct DummyWorker : public WORKER_TYPE {

    DummyWorker(ThinBlockManager& m, NodeId i) :
        WORKER_TYPE(m, i), isBuilt(false), buildStubCalled(false)
    { }

    virtual void buildStub(const StubData&, const TxFinder&) {
        buildStubCalled = true;
    }
    virtual bool isStubBuilt() const {
        return isBuilt;
    }
    bool isBuilt;
    bool buildStubCalled;
};

struct DummyHeaderProcessor : public BlockHeaderProcessor {

    DummyHeaderProcessor() : headerOK(true), called(false) { }

    bool operator()(const std::vector<CBlockHeader>&, bool, bool) override {
        called = true;
        return headerOK;
    }
    bool headerOK;
    bool called;
};

struct MerkleblockSetup {

    MerkleblockSetup() :
        mstream(SER_NETWORK, PROTOCOL_VERSION),
        tmgr(std::unique_ptr<ThinBlockFinishedCallb>(new DummyFinishedCallb),
             std::unique_ptr<InFlightEraser>(new DummyInFlightEraser))
    {
        CBloomFilter emptyFilter;
        mblock = CMerkleBlock(TestBlock2(), emptyFilter);
        mstream << mblock;

        // test assert when pushing ping to pfrom if consensus params
        // are not set.
        SelectParams(CBaseChainParams::MAIN);

        // asserts if fPrintToDebugLog is true
        fPrintToDebugLog = false;
    }
    CMerkleBlock mblock;
    CDataStream mstream;
    DummyNode pfrom;
    ThinBlockManager tmgr;
    DummyHeaderProcessor headerprocessor;
};

BOOST_FIXTURE_TEST_SUITE(process_merkleblock_tests, MerkleblockSetup);

BOOST_AUTO_TEST_CASE(ignore_if_has_block_data) {
    // Add block to mapBlockIndex (so we already have it)
    DummyBlockIndexEntry dummyEntry(mblock.header.GetHash());

    BloomThinWorker worker(tmgr, 42);
    worker.setToWork(mblock.header.GetHash());
    ProcessMerkleBlock(pfrom, mstream, worker, NullFinder(), headerprocessor);

    // peer should not be working on anything
    BOOST_CHECK(worker.isAvailable());
    BOOST_CHECK_EQUAL(0, pfrom.thinBlockNonce);
}

BOOST_AUTO_TEST_CASE(ditches_old_block) {
    // if we thought the peer was working on a block, but then
    // provided a new one, we should switch it over to the new one.
    uint256 dummyhash;
    dummyhash.SetHex("0xBADF00D");
    BloomThinWorker worker(tmgr, 42);
    worker.setToWork(dummyhash);
    ProcessMerkleBlock(pfrom, mstream, worker, NullFinder(), headerprocessor);

    BOOST_CHECK_EQUAL(mblock.header.GetHash().ToString(),
            worker.blockStr());
    BOOST_CHECK(pfrom.thinBlockNonce != 0);
    BOOST_CHECK(worker.isStubBuilt());
}

// We want to call build stub even if we have one.
// MerkleBlock's have full uint256 hash list, so they
// should replace xthin uint64_t hash list (NYI).
BOOST_AUTO_TEST_CASE(rebuild_stub) {
    DummyWorker<BloomThinWorker> worker(tmgr, 42);
    worker.setToWork(mblock.header.GetHash());
    worker.isBuilt = true;
    ProcessMerkleBlock(pfrom, mstream, worker, NullFinder(), headerprocessor);

    BOOST_CHECK(worker.buildStubCalled);
    BOOST_CHECK(pfrom.thinBlockNonce != 0);
}

BOOST_AUTO_TEST_CASE(build_stub) {
    DummyWorker<BloomThinWorker> worker(tmgr, 42);
    worker.setToWork(mblock.header.GetHash());
    worker.isBuilt = false;
    ProcessMerkleBlock(pfrom, mstream, worker, NullFinder(), headerprocessor);

    BOOST_CHECK(worker.buildStubCalled);
    BOOST_CHECK(pfrom.thinBlockNonce != 0);
}

BOOST_AUTO_TEST_CASE(merkleblock_ignore_if_supports_xthin) {
    pfrom.nServices = NODE_THIN;

    DummyWorker<BloomThinWorker> worker(tmgr, 42);
    worker.setToWork(mblock.header.GetHash());
    worker.isBuilt = false;
    ProcessMerkleBlock(pfrom, mstream, worker, NullFinder(), headerprocessor);

    BOOST_CHECK(!worker.buildStubCalled);
    BOOST_CHECK_EQUAL(0, pfrom.thinBlockNonce);
}

BOOST_AUTO_TEST_CASE(merkleblock_header_is_processed) {
    DummyWorker<BloomThinWorker> worker(tmgr, 42);
    worker.setToWork(mblock.header.GetHash());
    ProcessMerkleBlock(pfrom, mstream, worker, NullFinder(), headerprocessor);
    BOOST_CHECK(headerprocessor.called);
    BOOST_CHECK(worker.buildStubCalled);
    BOOST_CHECK(!worker.isAvailable());
}

BOOST_AUTO_TEST_CASE(merkleblock_stop_if_header_fails) {
    DummyWorker<BloomThinWorker> worker(tmgr, 42);
    worker.setToWork(mblock.header.GetHash());
    headerprocessor.headerOK = false;
    ProcessMerkleBlock(pfrom, mstream, worker, NullFinder(), headerprocessor);
    BOOST_CHECK(worker.isAvailable());
    BOOST_CHECK(!worker.buildStubCalled);
}

BOOST_AUTO_TEST_SUITE_END();

struct XThinBlockSetup {

    XThinBlockSetup() :
        tmgr(std::unique_ptr<ThinBlockFinishedCallb>(new DummyFinishedCallb),
             std::unique_ptr<InFlightEraser>(new DummyInFlightEraser))
    {
        SelectParams(CBaseChainParams::MAIN);
        fPrintToDebugLog = false;
    }

    CDataStream stream(const XThinBlock& block) {
        CDataStream stream(SER_NETWORK, PROTOCOL_VERSION);

        stream << block;
        return stream;;
    }

    XThinBlock xblock;
    DummyNode pfrom;
    ThinBlockManager tmgr;
    DummyHeaderProcessor headerprocessor;
};

struct DummyXThinProcessor : public XThinBlockProcessor {

    DummyXThinProcessor(CNode& f, ThinBlockWorker& w,
        BlockHeaderProcessor& h) : XThinBlockProcessor(f, w, h), misbehaved(0)
    { }

    virtual void misbehave(int howmuch) {
        misbehaved += howmuch;
    }
    int misbehaved;
};

BOOST_FIXTURE_TEST_SUITE(process_xthinblock_tests, XThinBlockSetup);

BOOST_AUTO_TEST_CASE(xthinblock_ignore_invalid) {
    XThinBlock xblock(TestBlock1(), CBloomFilter());;
    xblock.txHashes.clear(); // <- makes block invalid

    DummyWorker<XThinWorker> worker(tmgr, 42);
    worker.setToWork(xblock.header.GetHash());
    CDataStream s = stream(xblock);
    DummyXThinProcessor process(pfrom, worker, headerprocessor);
    process(s, NullFinder());

    // Should reset the worker.
    BOOST_CHECK(worker.isAvailable());

    // ...and misbehave the client.
    BOOST_CHECK_EQUAL(20, process.misbehaved);
    BOOST_CHECK(!worker.buildStubCalled);
    BOOST_CHECK_EQUAL("reject", pfrom.messages.at(0));
}

BOOST_AUTO_TEST_CASE(xthinblock_ignore_if_has_block_data) {
    // Add block to mapBlockIndex (so we already have it)
    XThinBlock xblock(TestBlock1(), CBloomFilter()); // <- has only coinbase
    DummyBlockIndexEntry dummyEntry(xblock.header.GetHash());

    DummyWorker<XThinWorker> worker(tmgr, 42);
    worker.setToWork(xblock.header.GetHash());
    CDataStream s = stream(xblock);
    DummyXThinProcessor process(pfrom, worker, headerprocessor);
    process(s, NullFinder());

    // peer should not be working on anything
    BOOST_CHECK(worker.isAvailable());
    BOOST_CHECK(pfrom.messages.empty()); //<- no re-requesting
}

BOOST_AUTO_TEST_CASE(xthinblock_ignore_if_not_requested) {
    DummyWorker<XThinWorker> worker(tmgr, 42);
    XThinBlock xblock(TestBlock1(), CBloomFilter());

    // set to work is not called, so it's not expecting xthinblock
    CDataStream s = stream(xblock);
    DummyXThinProcessor process(pfrom, worker, headerprocessor);
    process(s, NullFinder());
    BOOST_CHECK(worker.isAvailable());
    BOOST_CHECK(!worker.buildStubCalled);
}

BOOST_AUTO_TEST_CASE(xthinblock_header_is_processed) {
    XThinWorker worker(tmgr, 42);
    XThinBlock xblock(TestBlock1(), CBloomFilter());
    worker.setToWork(xblock.header.GetHash());
    CDataStream s = stream(xblock);
    DummyXThinProcessor process(pfrom, worker, headerprocessor);
    process(s, NullFinder());

    BOOST_CHECK(headerprocessor.called);
    BOOST_CHECK(worker.isStubBuilt());
    BOOST_CHECK(!worker.isAvailable());
}

BOOST_AUTO_TEST_CASE(xthinblock_stop_if_header_fails) {
    XThinWorker worker(tmgr, 42);
    XThinBlock xblock(TestBlock1(), CBloomFilter());
    worker.setToWork(xblock.header.GetHash());
    headerprocessor.headerOK = false;
    CDataStream s = stream(xblock);
    DummyXThinProcessor process(pfrom, worker, headerprocessor);
    process(s, NullFinder());

    BOOST_CHECK(worker.isAvailable());
    BOOST_CHECK(!worker.isStubBuilt());
}

BOOST_AUTO_TEST_CASE(xthinblock_rerequest_missing) {
    CBlock testblock = TestBlock1();

    // bloom filter will match all, so non will be included.
    XThinBlock xblock(testblock, CBloomFilter());
    XThinWorker worker(tmgr, 42);

    worker.setToWork(xblock.header.GetHash());
    CDataStream s = stream(xblock);
    DummyXThinProcessor process(pfrom, worker, headerprocessor);
    process(s, NullFinder());

    BOOST_CHECK(headerprocessor.called);
    BOOST_CHECK(worker.isStubBuilt());
    BOOST_CHECK(!worker.isAvailable());

    // should have re-requested missing transaction
    BOOST_CHECK_EQUAL("get_xblocktx", pfrom.messages.at(0));

    // create a bloom filter that matches non, so we
    // don't have to re-request.
    CBloomFilter f;
    f.clear();
    pfrom.messages.clear();
    xblock = XThinBlock(TestBlock1(), f);
    s = stream(xblock);
    process(s, NullFinder());
    BOOST_CHECK(pfrom.messages.empty());
}

BOOST_AUTO_TEST_SUITE_END();
BOOST_AUTO_TEST_SUITE(compactblockprocessor_tests);

// TODO: Move to compactblockprocessor_tests.cpp after thin block
// announcements are merged.
BOOST_AUTO_TEST_CASE(compactblockprocessor_fetch_full) {
    CBlock testblock = TestBlock1();

    DummyNode node;
    std::unique_ptr<ThinBlockManager> tmgr = GetDummyThinBlockMg();
    CompactWorker worker(*tmgr, node.id);
    DummyHeaderProcessor hprocessor;
    CompactBlockProcessor p(node, worker, hprocessor);

    // create a invalid compact block (obviously too big)
    CompactBlock block(TestBlock1(), CoinbaseOnlyPrefiller{});

    uint64_t currMaxBlockSize = 1000 * 100; // 100kb (for faster unittest)
    size_t tooManyTx = std::ceil(currMaxBlockSize * 1.05 / minTxSize()) + 1;
    block.prefilledtxn.resize(tooManyTx, block.prefilledtxn.at(0));

    CTxMemPool mpool(CFeeRate(0));
    CDataStream stream(SER_NETWORK, PROTOCOL_VERSION);
    stream << block;
    p(stream, mpool, currMaxBlockSize);

    // should have rejected the block
    BOOST_CHECK_EQUAL(1, node.messages.size());
    BOOST_CHECK_EQUAL("reject", node.messages.at(0));
}

BOOST_AUTO_TEST_SUITE_END();
