#include "process_merkleblock.h"
#include "bloom.h"
#include "chain.h"
#include "net.h"
#include "thinblockbuilder.h"
#include "main.h"
#include "merkleblock.h"
#include "chainparams.h"
#include "util.h" // for fPrintToDebugLog
#include "inflightindex.h"
#include "test/thinblockutil.h"
#include <boost/test/unit_test_suite.hpp>
#include <boost/test/test_tools.hpp>

struct DummyBlockIndexEntry {
DummyBlockIndexEntry(const uint256& hash) : hash(hash) {
    index.nStatus = BLOCK_HAVE_DATA;
    mapBlockIndex.insert(std::make_pair(hash, &index));
    }
    ~DummyBlockIndexEntry() {
        mapBlockIndex.erase(hash);
    }
    CBlockIndex index;
    uint256 hash;
};

struct MerkleblockSetup {

    MerkleblockSetup() :
        mstream(SER_NETWORK, PROTOCOL_VERSION),
        tmgr(std::auto_ptr<ThinBlockFinishedCallb>(new DummyFinishedCallb),
             std::auto_ptr<InFlightEraser>(new DummyInFlightEraser))
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
};

BOOST_FIXTURE_TEST_SUITE(processmessage_tests, MerkleblockSetup);

BOOST_AUTO_TEST_CASE(ignore_if_has_block_data) {
    // Add block to mapBlockIndex (so we already have it)
    DummyBlockIndexEntry dummyEntry(mblock.header.GetHash());

    ThinBlockWorker worker(tmgr, 42);
    worker.setToWork(mblock.header.GetHash());
    ProcessMerkleBlock(pfrom, mstream, worker, NullFinder());

    // peer should not be working on anything
    BOOST_CHECK(worker.isAvailable());
    BOOST_CHECK_EQUAL(0, pfrom.thinBlockNonce);
}

BOOST_AUTO_TEST_CASE(ditches_old_block) {
    // if we thought the peer was working on a block, but then
    // provided a new one, we should switch it over to the new one.
    uint256 dummyhash;
    dummyhash.SetHex("0xBADF00D");
    ThinBlockWorker worker(tmgr, 42);
    worker.setToWork(dummyhash);
    ProcessMerkleBlock(pfrom, mstream, worker, NullFinder());

    BOOST_CHECK_EQUAL(mblock.header.GetHash().ToString(),
            worker.blockStr());
    BOOST_CHECK(pfrom.thinBlockNonce != 0);
    BOOST_CHECK(worker.isStubBuilt());
}

struct DummyWorker : public ThinBlockWorker {

    DummyWorker(ThinBlockManager& m, NodeId i) :
        ThinBlockWorker(m, i), isBuilt(false), buildStubCalled(false)
    { }
    virtual void buildStub(
        const CMerkleBlock& m, const TxFinder& txFinder) {
        buildStubCalled = true;
    }
    virtual bool isStubBuilt() const {
        return isBuilt;
    }
    bool isBuilt;
    bool buildStubCalled;
};

BOOST_AUTO_TEST_CASE(dont_rebuild_stub) {
    DummyWorker worker(tmgr, 42);
    worker.setToWork(mblock.header.GetHash());
    worker.isBuilt = true;
    ProcessMerkleBlock(pfrom, mstream, worker, NullFinder());

    BOOST_CHECK(!worker.buildStubCalled);
    BOOST_CHECK(pfrom.thinBlockNonce != 0);
}

BOOST_AUTO_TEST_CASE(build_stub) {
    DummyWorker worker(tmgr, 42);
    worker.setToWork(mblock.header.GetHash());
    worker.isBuilt = false;
    ProcessMerkleBlock(pfrom, mstream, worker, NullFinder());

    BOOST_CHECK(worker.buildStubCalled);
    BOOST_CHECK(pfrom.thinBlockNonce != 0);
}

BOOST_AUTO_TEST_SUITE_END();

