#include <boost/test/unit_test_suite.hpp>
#include <boost/test/test_tools.hpp>
#include "test/thinblockutil.h"
#include "blockencodings.h"
#include "bloomthin.h"
#include "chainparams.h"
#include "compactthin.h"
#include "merkleblock.h"
#include "streams.h"
#include "thinblockconcluder.h"
#include "uint256.h"
#include "util.h" // for fPrintToDebugLog
#include "xthin.h"
#include <memory>

struct DummyBloomConcluder : public BloomBlockConcluder {

    DummyBloomConcluder() :
        BloomBlockConcluder(inFlight),
        reRequestCalled(0), giveUpCalled(0),
        fallbackDownloadCalled(0), misbehave(0)
    {
    }

    virtual void giveUp(CNode* pfrom, ThinBlockWorker& worker) {
        giveUpCalled++;
        BloomBlockConcluder::giveUp(pfrom, worker);
    }

    virtual void reRequest(
        CNode* pfrom,
        ThinBlockWorker& worker,
        uint64_t nonce)
    {
        reRequestCalled++;
        BloomBlockConcluder::reRequest(pfrom, worker, nonce);
    }
    virtual void fallbackDownload(CNode *pfrom, const uint256& block) {
        fallbackDownloadCalled++;
        BloomBlockConcluder::fallbackDownload(pfrom, block);
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
        tmgr(std::unique_ptr<ThinBlockFinishedCallb>(new DummyFinishedCallb),
             std::unique_ptr<InFlightEraser>(new DummyInFlightEraser))
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

    BloomThinWorker worker(tmgr, 42);
    worker.setAvailable();

    DummyBloomConcluder c;
    c(&pfrom, nonce, worker);
    BOOST_CHECK_EQUAL(0, c.giveUpCalled);
    BOOST_CHECK_EQUAL(0, c.reRequestCalled);
}

// Peer did not provide us the merkleblock.
// This should only happen if the peer does not support bloom filter.
BOOST_AUTO_TEST_CASE(merkleblock_not_provided) {
    BloomThinWorker worker(tmgr, 42);
    uint256 dummyhash;
    dummyhash.SetHex("0xDEADBEA7");
    pfrom.thinBlockNonceBlock = dummyhash;
    worker.setToWork(dummyhash);
    DummyBloomConcluder c;
    c(&pfrom, nonce, worker);

    BOOST_CHECK_EQUAL(0, c.giveUpCalled);
    BOOST_CHECK_EQUAL(0, c.reRequestCalled);
    BOOST_CHECK_EQUAL(20, c.misbehave);
    BOOST_CHECK(worker.isAvailable());
}

// Peer does not provide all transactions. We re-request them,
// and peer is able to provide.
BOOST_AUTO_TEST_CASE(rerequest_success) {
    BloomThinWorker worker(tmgr, 42);
    pfrom.thinBlockNonceBlock = mblock.header.GetHash();
    worker.setToWork(mblock.header.GetHash());
    worker.buildStub(ThinBloomStub(mblock), NullFinder());
    BOOST_CHECK(!worker.isReRequesting());

    DummyBloomConcluder c;
    c(&pfrom, nonce, worker);
    BOOST_CHECK_EQUAL(1, c.reRequestCalled);
    BOOST_CHECK_EQUAL(0, c.giveUpCalled);
    BOOST_CHECK(worker.isReRequesting());
    BOOST_CHECK_EQUAL(mblock.header.GetHash().ToString(), pfrom.thinBlockNonceBlock.ToString());
    BOOST_CHECK_EQUAL(nonce, pfrom.thinBlockNonce);

    // Provide all the transactions.
    std::vector<CTransaction> txs = block.vtx;
    typedef std::vector<CTransaction>::const_iterator auto_;
    for (auto_ t = txs.begin(); t != txs.end(); ++t)
        BOOST_CHECK(worker.addTx(*t));

    DummyBloomConcluder c2;
    c2(&pfrom, nonce, worker);
    BOOST_CHECK_EQUAL(0, c2.reRequestCalled);
    BOOST_CHECK_EQUAL(0, c2.giveUpCalled);
    BOOST_CHECK(worker.isAvailable());
    BOOST_CHECK_EQUAL(0, pfrom.thinBlockNonce);
}

BOOST_AUTO_TEST_CASE(re_request_not_fulfilled_one_worker) {
    CBloomFilter emptyFilter;

    BloomThinWorker worker(tmgr, 42);
    worker.setToWork(mblock.header.GetHash());
    worker.buildStub(ThinBloomStub(mblock), NullFinder());
    worker.setReRequesting(true);
    pfrom.thinBlockNonceBlock = mblock.header.GetHash();

    // We have re-requested the missing transactions, but block is still incomplete.
    DummyBloomConcluder c;
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

    BloomThinWorker worker1(tmgr, 42);
    BloomThinWorker worker2(tmgr, 24);
    worker1.setToWork(mblock.header.GetHash());
    worker2.setToWork(mblock.header.GetHash());
    worker1.buildStub(ThinBloomStub(mblock), NullFinder());
    worker1.setReRequesting(true);
    worker2.setReRequesting(true);
    pfrom.thinBlockNonceBlock = mblock.header.GetHash();

    DummyBloomConcluder c;

    // First worker to give up - no need to fall back to full download.
    c(&pfrom, nonce, worker1);
    BOOST_CHECK_EQUAL(0, c.misbehave);
    BOOST_CHECK_EQUAL(0, c.reRequestCalled);
    BOOST_CHECK_EQUAL(1, c.giveUpCalled);
    BOOST_CHECK_EQUAL(0, c.fallbackDownloadCalled);


    // Second (and last) worker to give up. Fall back to full download.
    pfrom.thinBlockNonceBlock = mblock.header.GetHash();
    c(&pfrom, nonce, worker2);

    BOOST_CHECK_EQUAL(0, c.misbehave);
    BOOST_CHECK_EQUAL(0, c.reRequestCalled);
    BOOST_CHECK_EQUAL(2, c.giveUpCalled);
    BOOST_CHECK_EQUAL(1, c.fallbackDownloadCalled);
}

BOOST_AUTO_TEST_CASE(ignore_old_pongs) {
    uint256 dummyhash1, dummyhash2;
    dummyhash1.SetHex("0xBADF00D");
    dummyhash2.SetHex("0xBADBEEF");

    pfrom.thinBlockNonce = nonce;
    pfrom.thinBlockNonceBlock = dummyhash1;
    BloomThinWorker worker(tmgr, 42);
    worker.setToWork(dummyhash2); // working on next block
    DummyBloomConcluder c;
    c(&pfrom, pfrom.thinBlockNonce, worker);

    // Should have been ignored.
    BOOST_CHECK_EQUAL(0, c.reRequestCalled);
    BOOST_CHECK_EQUAL(0, c.giveUpCalled);
    BOOST_CHECK_EQUAL(nonce, pfrom.thinBlockNonce);
    BOOST_CHECK_EQUAL(dummyhash1.ToString(), pfrom.thinBlockNonceBlock.ToString());
}

BOOST_AUTO_TEST_CASE(xthin_concluder) {

    XThinReReqResponse resp;
    resp.block = uint256S("0xBADBAD");
    resp.txRequested.push_back(CTransaction());

    struct DummyWorker : public XThinWorker {
        DummyWorker(ThinBlockManager& mg, NodeId id) :
            XThinWorker(mg, id), addTxCalled(false) { }

        bool addTx(const CTransaction& tx) override {
            addTxCalled = true;
            return true;
        }

        bool addTxCalled;
    };

    DummyWorker worker(tmgr, 42);
    XThinBlockConcluder conclude;
    // Should ignore since worker is not working
    // on anything.
    worker.setAvailable();
    conclude(resp, pfrom, worker);
    BOOST_CHECK(!worker.addTxCalled);

    // Should ignore since worker is assigned to a
    // different block.
    worker.setToWork(uint256S("0xf00d"));
    conclude(resp, pfrom, worker);
    BOOST_CHECK(!worker.addTxCalled);

    // Should add tx.
    worker.setToWork(resp.block);
    conclude(resp, pfrom, worker);
}

BOOST_AUTO_TEST_CASE(compact_concluder) {

    CompactReReqResponse resp;
    resp.blockhash = uint256S("0xBADBAD");
    resp.txn.push_back(CTransaction());

    struct DummyWorker : public CompactWorker {
        DummyWorker(ThinBlockManager& mg, NodeId id) :
            CompactWorker(mg, id), addTxCalled(false) { }

        bool addTx(const CTransaction& tx) override {
            addTxCalled = true;
            return true;
        }

        bool addTxCalled;
    };

    DummyWorker worker(tmgr, 42);
    CompactBlockConcluder conclude;
    // Should ignore since worker is not working
    // on anything.
    worker.setAvailable();
    conclude(resp, pfrom, worker);
    BOOST_CHECK(!worker.addTxCalled);

    // Should ignore since worker is assigned to a
    // different block.
    worker.setToWork(uint256S("0xf00d"));
    conclude(resp, pfrom, worker);
    BOOST_CHECK(!worker.addTxCalled);

    // Should add tx.
    worker.setToWork(resp.blockhash);
    conclude(resp, pfrom, worker);
};

BOOST_AUTO_TEST_SUITE_END();
