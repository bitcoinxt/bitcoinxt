// Copyright (c) 2016- The Bitcoin XT developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#include <boost/test/unit_test.hpp>
#include "blockheaderprocessor.h"
#include "test/thinblockutil.h" // DummyNode
#include "test/testutil.h"

BOOST_AUTO_TEST_SUITE(blockheaderprocessor_tests);

class DefaultHeaderProcessorDummy : public DefaultHeaderProcessor {
    public:

        DefaultHeaderProcessorDummy(CNode* pfrom,
                InFlightIndex& i, ThinBlockManager& tm, BlockInFlightMarker& m,
                std::function<void()> checkBlockIndex,
                std::function<void()> sendGetHeaders) :
            DefaultHeaderProcessor(pfrom, i, tm, m, checkBlockIndex, sendGetHeaders)
        {
        }

        std::tuple<bool, CBlockIndex*> acceptHeaders(
                const std::vector<CBlockHeader>& headers) override
        {
            return std::make_tuple(true, nullptr);
        }
};

BOOST_AUTO_TEST_CASE(test_connect_chain_req) {

    // Header does not connect, should
    // send a getheaders to connect and stop processing headers.

    bool checkBlockCalled = false;
    auto checkBlockFunc = [&checkBlockCalled]() {
        checkBlockCalled = true;
    };

    bool sendGetHeadersCalled = false;
    auto sendGetHeadersFunc = [&sendGetHeadersCalled]() {
        sendGetHeadersCalled = true;
    };

    DummyNode from;
    auto thinmg = GetDummyThinBlockMg();
    InFlightIndex inFlight;
    DummyMarkAsInFlight markAsInFlight;

    DefaultHeaderProcessorDummy processHeader(&from, inFlight, *thinmg,
        markAsInFlight, checkBlockFunc, sendGetHeadersFunc);

    // Try to process a header that does not connect
    DummyBlockIndexEntry e1(uint256S("0xaaa"));
    DummyBlockIndexEntry e2(uint256S("0xbbb"));

    CBlockHeader header;
    header.hashPrevBlock = uint256S("0xccc"); //< prev block not in mapBlockIndex
    processHeader({ header }, false, true);

    BOOST_CHECK(sendGetHeadersCalled);
    BOOST_CHECK(!checkBlockCalled);
    BOOST_CHECK_EQUAL(1, NodeStatePtr(from.id)->unconnectingHeaders);

    // Add entry so that header connects. Try again.
    sendGetHeadersCalled = false;
    checkBlockCalled = false;

    DummyBlockIndexEntry e3(header.hashPrevBlock);
    processHeader({ header }, false, true);
    BOOST_CHECK(!sendGetHeadersCalled);
    BOOST_CHECK(checkBlockCalled);
    BOOST_CHECK_EQUAL(0, NodeStatePtr(from.id)->unconnectingHeaders);
}

BOOST_AUTO_TEST_SUITE_END();
