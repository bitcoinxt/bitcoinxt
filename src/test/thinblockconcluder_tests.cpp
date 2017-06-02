#include <boost/test/unit_test_suite.hpp>
#include <boost/test/test_tools.hpp>
#include "test/thinblockutil.h"
#include "blockencodings.h"
#include "chainparams.h"
#include "compactthin.h"
#include "merkleblock.h"
#include "streams.h"
#include "thinblockconcluder.h"
#include "uint256.h"
#include "util.h" // for fPrintToDebugLog
#include "xthin.h"
#include <memory>

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
    DummyMarkAsInFlight markInFlight;
};


BOOST_FIXTURE_TEST_SUITE(thinblockconcluder_tests, ConcluderSetup);

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
    conclude(resp, pfrom, worker, markInFlight);
    BOOST_CHECK(!worker.addTxCalled);

    // Should ignore since worker is assigned to a
    // different block.
    worker.setToWork(uint256S("0xf00d"));
    conclude(resp, pfrom, worker, markInFlight);
    BOOST_CHECK(!worker.addTxCalled);

    // Should add tx.
    worker.setToWork(resp.block);
    conclude(resp, pfrom, worker, markInFlight);
    BOOST_CHECK(worker.addTxCalled);
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
    conclude(resp, pfrom, worker, markInFlight);
    BOOST_CHECK(!worker.addTxCalled);

    // Should ignore since worker is assigned to a
    // different block.
    worker.setToWork(uint256S("0xf00d"));
    conclude(resp, pfrom, worker, markInFlight);
    BOOST_CHECK(!worker.addTxCalled);

    // Should add tx.
    worker.setToWork(resp.blockhash);
    conclude(resp, pfrom, worker, markInFlight);
    BOOST_CHECK(worker.addTxCalled);
};

BOOST_AUTO_TEST_SUITE_END();
