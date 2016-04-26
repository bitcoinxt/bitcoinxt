// Copyright (c) 2016- The Bitcoin XT developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#include <boost/test/unit_test.hpp>

#include "test/thinblockutil.h"
#include "bloom.h"
#include "uint256.h"
#include "xthin.h"
#include "chainparams.h"


// Workaround for segfaulting
struct Workaround {
    Workaround() {
        SelectParams(CBaseChainParams::MAIN);
    }
};

BOOST_FIXTURE_TEST_SUITE(xthin_tests, Workaround);

BOOST_AUTO_TEST_CASE(empty_bloom) {
    CBlock b = TestBlock1();
    CBloomFilter emptyFilter;
    emptyFilter.clear();

    // Filter none
    XThinBlock thinb(b, emptyFilter);

    BOOST_CHECK(thinb.header.GetHash() == b.GetBlockHeader().GetHash());
    BOOST_CHECK(thinb.txHashes.size() == b.vtx.size());
    BOOST_ASSERT(thinb.missing.size() == b.vtx.size());

    typedef std::vector<CTransaction>::const_iterator auto_;
    auto_ m = thinb.missing.begin();
    auto_ v = b.vtx.begin();
    for (; m != thinb.missing.end(); ++m, ++v)
        BOOST_CHECK_EQUAL(m->GetHash().ToString(), v->GetHash().ToString());
}

BOOST_AUTO_TEST_CASE(only_coinbase) {
    CBlock b = TestBlock1();
    CBloomFilter filter(b.vtx.size(), 0.000001, insecure_rand(), BLOOM_UPDATE_ALL);

    // Filter out all
    typedef std::vector<CTransaction>::const_iterator auto_;
    for (auto_ t = b.vtx.begin(); t != b.vtx.end(); ++t)
        filter.insert(t->GetHash());

    // Should still get coinbase tx.
    XThinBlock thinb(b, filter);
    BOOST_CHECK_EQUAL(thinb.txHashes.size(), b.vtx.size());
    BOOST_CHECK_EQUAL(1, thinb.missing.size());
    BOOST_CHECK_EQUAL(
            b.vtx[0].GetHash().ToString(),
            thinb.missing[0].GetHash().ToString());

}

BOOST_AUTO_TEST_CASE(filter_some) {

    CBlock b = TestBlock1();
    CBloomFilter filter(b.vtx.size(), 0.000001, insecure_rand(), BLOOM_UPDATE_ALL);

    filter.insert(b.vtx[1].GetHash());
    filter.insert(b.vtx[2].GetHash());
    filter.insert(b.vtx[3].GetHash());

    XThinBlock thinb(b, filter);

    BOOST_CHECK_EQUAL(b.vtx.size(), thinb.txHashes.size());
    BOOST_CHECK_EQUAL(b.vtx.size() - 3, thinb.missing.size());
    bool found = std::find(thinb.missing.begin(),
            thinb.missing.end(), b.vtx[1]) != thinb.missing.end();
    BOOST_CHECK(!found);

}

BOOST_AUTO_TEST_CASE(throw_on_collision) {
    CBlock b = TestBlock1();
    CBloomFilter filter;
    b.vtx[1] = b.vtx[2]; // create collision

    BOOST_CHECK_THROW(XThinBlock(b, filter), xthin_collision_error);
}

struct ThinBlockMgDummy : public ThinBlockManager {
    ThinBlockMgDummy() : ThinBlockManager(
            std::unique_ptr<ThinBlockFinishedCallb>(),
            std::unique_ptr<InFlightEraser>())
    {
    }
};

struct NullProvider : public TxHashProvider {
    virtual void operator()(std::vector<uint256>& dst) {
    }
};

BOOST_AUTO_TEST_CASE(requestBlock) {
    ThinBlockMgDummy mg;
    DummyNode node;
    XThinWorker w(mg, node.id, std::unique_ptr<TxHashProvider>(new NullProvider));

    std::vector<CInv> reqs;
    w.requestBlock(uint256S("0xfafafa"), reqs, node);

    // xthin does not add a generic CInv getdata request
    BOOST_CHECK(reqs.empty());

    // ... it pushes its own custom message
    BOOST_CHECK_EQUAL("get_xthin", node.messages.at(0));
}

BOOST_AUTO_TEST_CASE(xthin_stub_self_validate) {
    // ok
    {
        XThinBlock xblock(TestBlock1(), CBloomFilter());
        BOOST_CHECK_NO_THROW(xblock.selfValidate());
    }

    // missing coinbase
    {
        XThinBlock xblock(TestBlock1(), CBloomFilter());
        xblock.missing.clear();
        BOOST_CHECK_THROW(xblock.selfValidate(), std::invalid_argument);
    }

    // more transactions bundled than are in block
    {
        CBloomFilter matchNone;
        matchNone.clear();
        XThinBlock xblock(TestBlock1(), matchNone);
        xblock.missing.push_back(CTransaction());
        BOOST_CHECK_THROW(xblock.selfValidate(), std::invalid_argument);
    }

    // hash collision
    {
        XThinBlock xblock(TestBlock1(), CBloomFilter(), false);
        xblock.txHashes[1] = xblock.txHashes[2];
        BOOST_CHECK_THROW(xblock.selfValidate(), std::invalid_argument);
    }
}

BOOST_AUTO_TEST_CASE(xthin_req_response) {

    CBlock block = TestBlock2();
    std::set<uint64_t> requesting;
    requesting.insert(block.vtx[1].GetHash().GetCheapHash());
    requesting.insert(block.vtx[2].GetHash().GetCheapHash());
    XThinReReqResponse resp(block, requesting);

    BOOST_CHECK_EQUAL(static_cast<size_t>(2), resp.txRequested.size());
    BOOST_CHECK(block.vtx[1] == resp.txRequested[0]);
    BOOST_CHECK(block.vtx[2] == resp.txRequested[1]);
};

BOOST_AUTO_TEST_SUITE_END();
