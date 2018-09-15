#include "nodestate.h"
#include "test/test_bitcoin.h"
#include "test/thinblockutil.h"

#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(nodestate_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(update_best_from_last) {
    uint256 unknown_block(uint256S("0x1"));
    // known blocks
    uint256 no_work(uint256S("0x2"));
    uint256 some_work(uint256S("0x3"));
    uint256 more_work(uint256S("0x4"));
    uint256 equal_work(uint256S("0x5"));
    uint256 less_work(uint256S("0x6"));

    CBlockIndex no_work_i;
    no_work_i.nChainWork = 0;
    no_work_i.phashBlock = &no_work;
    CBlockIndex some_work_i;
    some_work_i.nChainWork = 100;
    some_work_i.phashBlock = &some_work;
    CBlockIndex more_work_i;
    more_work_i.nChainWork = some_work_i.nChainWork + 100;
    more_work_i.phashBlock = &more_work;
    CBlockIndex equal_work_i;
    equal_work_i.nChainWork = more_work_i.nChainWork;
    equal_work_i.phashBlock = &equal_work;
    CBlockIndex less_work_i;
    less_work_i.nChainWork = equal_work_i.nChainWork - 50;
    less_work_i.phashBlock = &less_work;

    BlockMap chainIndex;
    chainIndex[no_work] = &no_work_i;
    chainIndex[some_work] = &some_work_i;
    chainIndex[more_work] = &more_work_i;
    chainIndex[equal_work] = &equal_work_i;
    chainIndex[less_work] = &less_work_i;

    auto mgr = GetDummyThinBlockMg();
    CNodeState state(42, *mgr, CService{}, "dummy");

    // unknown is null
    assert(state.hashLastUnknownBlock.IsNull());
    BOOST_CHECK(!state.UpdateBestFromLast(chainIndex));

    // unknown is... unknown
    state.hashLastUnknownBlock = unknown_block;
    BOOST_CHECK(!state.UpdateBestFromLast(chainIndex));
    BOOST_CHECK(unknown_block == state.hashLastUnknownBlock); // still unknown

    // unknown is known, but has no chainwork
    state.hashLastUnknownBlock = no_work;
    BOOST_CHECK(!state.UpdateBestFromLast(chainIndex));
    BOOST_CHECK(state.hashLastUnknownBlock.IsNull()); // now known
    BOOST_CHECK(state.pindexBestKnownBlock == nullptr);

    // unknown is known, and has chainwork
    state.hashLastUnknownBlock = some_work;
    BOOST_CHECK(state.UpdateBestFromLast(chainIndex));
    BOOST_CHECK(state.hashLastUnknownBlock.IsNull());
    BOOST_CHECK(state.pindexBestKnownBlock == &some_work_i);

    // a better block becomes known
    state.hashLastUnknownBlock = more_work;
    BOOST_CHECK(state.UpdateBestFromLast(chainIndex));
    BOOST_CHECK(state.hashLastUnknownBlock.IsNull());
    BOOST_CHECK(state.pindexBestKnownBlock == &more_work_i);

    // an equal block becomes known, that's interesting, so we change best known
    // to it.
    state.hashLastUnknownBlock = equal_work;
    BOOST_CHECK(state.UpdateBestFromLast(chainIndex));
    BOOST_CHECK(state.hashLastUnknownBlock.IsNull());
    BOOST_CHECK(state.pindexBestKnownBlock == &equal_work_i);

    // less work block comes along
    state.hashLastUnknownBlock = less_work;
    BOOST_CHECK(!state.UpdateBestFromLast(chainIndex));
    BOOST_CHECK(state.hashLastUnknownBlock.IsNull());
    BOOST_CHECK(state.pindexBestKnownBlock != &less_work_i);
}

BOOST_AUTO_TEST_SUITE_END()
