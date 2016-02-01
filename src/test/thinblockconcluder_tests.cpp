#include <boost/test/unit_test_suite.hpp>
#include <boost/test/test_tools.hpp>
#include "test/thinblockutil.h"
#include "merkleblock.h"
#include "streams.h"
#include "thinblockconcluder.h"
#include "uint256.h"
#include "chainparams.h"
#include "util.h" // for fPrintToDebugLog
#include <memory>

struct DummyMarkAsInFlight : public BlockInFlightMarker {

    virtual void operator()(
        NodeId nodeid, const uint256& hash,
        const Consensus::Params& consensusParams,
        CBlockIndex *pindex)
    {
        block = hash;
    }
    uint256 block;
};

struct DummyConcluder : public ThinBlockConcluder {

    DummyConcluder() :
        ThinBlockConcluder(inFlight),
        reRequestCalled(0), giveUpCalled(0),
        fallbackDownloadCalled(0), misbehave(0)
    {
    }

    virtual void giveUp(CNode* pfrom, ThinBlockWorker& worker) {
        giveUpCalled++;
        ThinBlockConcluder::giveUp(pfrom, worker);
    }

    virtual void reRequest(
        CNode* pfrom,
        ThinBlockWorker& worker,
        uint64_t nonce)
    {
        reRequestCalled++;
        ThinBlockConcluder::reRequest(pfrom, worker, nonce);
    }
    virtual void fallbackDownload(CNode *pfrom, const uint256& block) {
        fallbackDownloadCalled++;
        ThinBlockConcluder::fallbackDownload(pfrom, block);
    }
    virtual void misbehaving(NodeId, int howmuch) {
        misbehave = howmuch;
    }

    int reRequestCalled;
    int giveUpCalled;
    int fallbackDownloadCalled;
    int misbehave;
    DummyMarkAsInFlight inFlight;
};

struct ConcluderSetup {

    ConcluderSetup() :
        mstream(SER_NETWORK, PROTOCOL_VERSION),
        tmgr(std::auto_ptr<ThinBlockFinishedCallb>(new DummyFinishedCallb),
             std::auto_ptr<InFlightEraser>(new DummyInFlightEraser))
    {
        CBloomFilter emptyFilter;
        block = TestBlock2();
        mblock = CMerkleBlock(block, emptyFilter);
        mstream << mblock;

        SelectParams(CBaseChainParams::MAIN);

        // asserts if fPrintToDebugLog is true
        fPrintToDebugLog = false;
        nonce = 0xBADFEE;
    }
    CBlock block;
    CMerkleBlock mblock;
    CDataStream mstream;
    DummyNode pfrom;
    ThinBlockManager tmgr;
    uint64_t nonce;
};


BOOST_FIXTURE_TEST_SUITE(thinblockconcluder_tests, ConcluderSetup);

// The normal case. Block is finished (and worker is available).
BOOST_AUTO_TEST_CASE(block_complete) {

    ThinBlockWorker worker(tmgr, 42);
    worker.setAvailable();

    DummyConcluder c;
    c(&pfrom, nonce, worker);
    BOOST_CHECK_EQUAL(0, c.giveUpCalled);
    BOOST_CHECK_EQUAL(0, c.reRequestCalled);
}

// Peer did not provide us the merkleblock.
// This should only happen if the peer does not support bloom filter.
BOOST_AUTO_TEST_CASE(merkleblock_not_provided) {
    ThinBlockWorker worker(tmgr, 42);
    uint256 dummyhash;
    dummyhash.SetHex("0xDEADBEA7");
    worker.setToWork(dummyhash);
    DummyConcluder c;
    c(&pfrom, nonce, worker);

    BOOST_CHECK_EQUAL(0, c.giveUpCalled);
    BOOST_CHECK_EQUAL(0, c.reRequestCalled);
    BOOST_CHECK_EQUAL(20, c.misbehave);
    BOOST_CHECK(worker.isAvailable());
}

// Peer does not provide all transactions. We re-request them,
// and peer is able to provide.
BOOST_AUTO_TEST_CASE(rerequest_success) {
    ThinBlockWorker worker(tmgr, 42);
    worker.setToWork(mblock.header.GetHash());
    worker.buildStub(mblock, NullFinder());
    BOOST_CHECK(!worker.isReRequesting());

    DummyConcluder c;
    c(&pfrom, nonce, worker);
    BOOST_CHECK_EQUAL(1, c.reRequestCalled);
    BOOST_CHECK_EQUAL(0, c.giveUpCalled);
    BOOST_CHECK(worker.isReRequesting());
    BOOST_CHECK_EQUAL(nonce, pfrom.thinBlockNonce);

    // Provide all the transactions.
    std::vector<CTransaction> txs = block.vtx;
    typedef std::vector<CTransaction>::const_iterator auto_;
    for (auto_ t = txs.begin(); t != txs.end(); ++t)
        worker.addTx(*t);

    DummyConcluder c2;
    c2(&pfrom, nonce, worker);
    BOOST_CHECK_EQUAL(0, c2.reRequestCalled);
    BOOST_CHECK_EQUAL(0, c2.giveUpCalled);
    BOOST_CHECK(worker.isAvailable());
    BOOST_CHECK_EQUAL(0, pfrom.thinBlockNonce);
}

BOOST_AUTO_TEST_CASE(re_request_not_fulfilled_one_worker) {
    CBloomFilter emptyFilter;

    ThinBlockWorker worker(tmgr, 42);
    worker.setToWork(mblock.header.GetHash());
    worker.buildStub(mblock, NullFinder());
    worker.setReRequesting(true);

    // We have re-requested the missing transactions, but block is still incomplete.
    DummyConcluder c;
    c(&pfrom, nonce, worker);

    // FIXME: Should we mark node as misbehaving? What effect would that
    // have on a big reorg?
    BOOST_CHECK_EQUAL(0, c.misbehave);
    BOOST_CHECK_EQUAL(0, c.reRequestCalled);
    BOOST_CHECK_EQUAL(1, c.giveUpCalled);
    BOOST_CHECK_EQUAL(1, c.fallbackDownloadCalled);
    BOOST_CHECK(mblock.header.GetHash() == c.inFlight.block);
}

BOOST_AUTO_TEST_CASE(re_request_not_fulfilled_multiple_workers) {
    CBloomFilter emptyFilter;
    ThinBlockBuilder bb = ThinBlockBuilder(mblock, NullFinder());

    ThinBlockWorker worker1(tmgr, 42);
    ThinBlockWorker worker2(tmgr, 24);
    worker1.setToWork(mblock.header.GetHash());
    worker2.setToWork(mblock.header.GetHash());
    worker1.buildStub(mblock, NullFinder());
    worker1.setReRequesting(true);
    worker2.setReRequesting(true);

    DummyConcluder c;

    // First worker to give up - no need to fall back to full download.
    c(&pfrom, nonce, worker1);
    BOOST_CHECK_EQUAL(0, c.misbehave);
    BOOST_CHECK_EQUAL(0, c.reRequestCalled);
    BOOST_CHECK_EQUAL(1, c.giveUpCalled);
    BOOST_CHECK_EQUAL(0, c.fallbackDownloadCalled);
    c(&pfrom, nonce, worker2);

    // Second (and last) worker to give up. Fall back to full download.
    BOOST_CHECK_EQUAL(0, c.misbehave);
    BOOST_CHECK_EQUAL(0, c.reRequestCalled);
    BOOST_CHECK_EQUAL(2, c.giveUpCalled);
    BOOST_CHECK_EQUAL(1, c.fallbackDownloadCalled);
}

BOOST_AUTO_TEST_SUITE_END();
