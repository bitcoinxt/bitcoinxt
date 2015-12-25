#include "process_merkleblock.h"
#include "bloom.h"
#include "chain.h"
#include "net.h"
#include "thinblockbuilder.h"
#include "main.h"
#include "merkleblock.h"
#include "chainparams.h"
#include "util.h" // for fPrintToDebugLog
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

struct DummyNode : public CNode {
    DummyNode() : CNode(INVALID_SOCKET, CAddress()) {
        id = 42;
    }
};

struct NullFinder : public TxFinder {
    virtual CTransaction operator()(const uint256& hash) const {
        return CTransaction();
    }
};

struct DummyCallb : ThinBlockFinishedCallb {
    virtual void operator()(const CBlock&, const std::vector<NodeId>&) { }
};

CBlock Block1();

struct MerkleblockSetup {

    MerkleblockSetup() :
        mstream(SER_NETWORK, PROTOCOL_VERSION),
        tmgr(std::auto_ptr<ThinBlockFinishedCallb>(new DummyCallb))
    {
        CBloomFilter emptyFilter;
        mblock = CMerkleBlock(Block1(), emptyFilter);
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

    ThinBlockWorker worker(tmgr, mblock.header.GetHash(), 42);
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
    ThinBlockWorker worker(tmgr, dummyhash, 42);
    ProcessMerkleBlock(pfrom, mstream, worker, NullFinder());

    BOOST_CHECK_EQUAL(mblock.header.GetHash().ToString(),
            worker.blockStr());
    BOOST_CHECK(pfrom.thinBlockNonce != 0);
    BOOST_CHECK(worker.isStubBuilt());
}

struct DummyWorker : public ThinBlockWorker {

    DummyWorker(ThinBlockManager& m, const uint256& b, NodeId i) :
        ThinBlockWorker(m, b, i), isBuilt(false), buildStubCalled(false)
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
    DummyWorker worker(tmgr, mblock.header.GetHash(), 42);
    worker.isBuilt = true;
    ProcessMerkleBlock(pfrom, mstream, worker, NullFinder());

    BOOST_CHECK(!worker.buildStubCalled);
    BOOST_CHECK(pfrom.thinBlockNonce != 0);
}

BOOST_AUTO_TEST_CASE(build_stub) {
    DummyWorker worker(tmgr, mblock.header.GetHash(), 42);
    worker.isBuilt = false;
    ProcessMerkleBlock(pfrom, mstream, worker, NullFinder());

    BOOST_CHECK(worker.buildStubCalled);
    BOOST_CHECK(pfrom.thinBlockNonce != 0);
}

BOOST_AUTO_TEST_SUITE_END();

CBlock Block1() {
    // Taken from bloom test merkle_block_2
    // Random real block (000000005a4ded781e667e06ceefafb71410b511fe0d5adc3e5a27ecbec34ae6)
    // With 4 txes
    CBlock block;
    CDataStream stream(ParseHex("0100000075616236cc2126035fadb38deb65b9102cc2c41c09cdf29fc051906800000000fe7d5e12ef0ff901f6050211249919b1c0653771832b3a80c66cea42847f0ae1d4d26e49ffff001d00f0a4410401000000010000000000000000000000000000000000000000000000000000000000000000ffffffff0804ffff001d029105ffffffff0100f2052a010000004341046d8709a041d34357697dfcb30a9d05900a6294078012bf3bb09c6f9b525f1d16d5503d7905db1ada9501446ea00728668fc5719aa80be2fdfc8a858a4dbdd4fbac00000000010000000255605dc6f5c3dc148b6da58442b0b2cd422be385eab2ebea4119ee9c268d28350000000049483045022100aa46504baa86df8a33b1192b1b9367b4d729dc41e389f2c04f3e5c7f0559aae702205e82253a54bf5c4f65b7428551554b2045167d6d206dfe6a2e198127d3f7df1501ffffffff55605dc6f5c3dc148b6da58442b0b2cd422be385eab2ebea4119ee9c268d2835010000004847304402202329484c35fa9d6bb32a55a70c0982f606ce0e3634b69006138683bcd12cbb6602200c28feb1e2555c3210f1dddb299738b4ff8bbe9667b68cb8764b5ac17b7adf0001ffffffff0200e1f505000000004341046a0765b5865641ce08dd39690aade26dfbf5511430ca428a3089261361cef170e3929a68aee3d8d4848b0c5111b0a37b82b86ad559fd2a745b44d8e8d9dfdc0cac00180d8f000000004341044a656f065871a353f216ca26cef8dde2f03e8c16202d2e8ad769f02032cb86a5eb5e56842e92e19141d60a01928f8dd2c875a390f67c1f6c94cfc617c0ea45afac0000000001000000025f9a06d3acdceb56be1bfeaa3e8a25e62d182fa24fefe899d1c17f1dad4c2028000000004847304402205d6058484157235b06028c30736c15613a28bdb768ee628094ca8b0030d4d6eb0220328789c9a2ec27ddaec0ad5ef58efded42e6ea17c2e1ce838f3d6913f5e95db601ffffffff5f9a06d3acdceb56be1bfeaa3e8a25e62d182fa24fefe899d1c17f1dad4c2028010000004a493046022100c45af050d3cea806cedd0ab22520c53ebe63b987b8954146cdca42487b84bdd6022100b9b027716a6b59e640da50a864d6dd8a0ef24c76ce62391fa3eabaf4d2886d2d01ffffffff0200e1f505000000004341046a0765b5865641ce08dd39690aade26dfbf5511430ca428a3089261361cef170e3929a68aee3d8d4848b0c5111b0a37b82b86ad559fd2a745b44d8e8d9dfdc0cac00180d8f000000004341046a0765b5865641ce08dd39690aade26dfbf5511430ca428a3089261361cef170e3929a68aee3d8d4848b0c5111b0a37b82b86ad559fd2a745b44d8e8d9dfdc0cac000000000100000002e2274e5fea1bf29d963914bd301aa63b64daaf8a3e88f119b5046ca5738a0f6b0000000048473044022016e7a727a061ea2254a6c358376aaa617ac537eb836c77d646ebda4c748aac8b0220192ce28bf9f2c06a6467e6531e27648d2b3e2e2bae85159c9242939840295ba501ffffffffe2274e5fea1bf29d963914bd301aa63b64daaf8a3e88f119b5046ca5738a0f6b010000004a493046022100b7a1a755588d4190118936e15cd217d133b0e4a53c3c15924010d5648d8925c9022100aaef031874db2114f2d869ac2de4ae53908fbfea5b2b1862e181626bb9005c9f01ffffffff0200e1f505000000004341044a656f065871a353f216ca26cef8dde2f03e8c16202d2e8ad769f02032cb86a5eb5e56842e92e19141d60a01928f8dd2c875a390f67c1f6c94cfc617c0ea45afac00180d8f000000004341046a0765b5865641ce08dd39690aade26dfbf5511430ca428a3089261361cef170e3929a68aee3d8d4848b0c5111b0a37b82b86ad559fd2a745b44d8e8d9dfdc0cac00000000"), SER_NETWORK, PROTOCOL_VERSION);
    stream >> block;
    return block;
}
