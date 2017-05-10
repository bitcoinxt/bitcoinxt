// Copyright (c) 2016- The Bitcoin XT developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#include <boost/test/unit_test.hpp>
#include "blockheaderprocessor.h"
#include "test/thinblockutil.h" // DummyNode
#include "test/testutil.h"

BOOST_AUTO_TEST_SUITE(blockheaderprocessor_tests);

BOOST_AUTO_TEST_CASE(test_connect_chain_req) {

    // Header does not connect, should
    // send a getheaders to connect and stop processing headers.

    DummyNode from;

    // Try to process a header that does not connect
    DummyBlockIndexEntry e1(uint256S("0xaaa"));
    DummyBlockIndexEntry e2(uint256S("0xbbb"));

    CBlockHeader header;
    header.hashPrevBlock = uint256S("0xccc"); //< prev block not in mapBlockIndex

    auto thinmg = GetDummyThinBlockMg();
    InFlightIndex inFlight;
    DummyMarkAsInFlight markAsInFlight;
    auto checkBlockIndex = []() { };

    DefaultHeaderProcessor p(&from, inFlight, *thinmg, markAsInFlight, checkBlockIndex);

    BOOST_CHECK(p.requestConnectHeaders(header, from));
    BOOST_CHECK_EQUAL(1, NodeStatePtr(from.id)->unconnectingHeaders);
    BOOST_CHECK_EQUAL(1, from.messages.size());
    BOOST_CHECK_EQUAL("getheaders", from.messages.at(0));

    // Add entry so that header connects. Try again.

    DummyBlockIndexEntry e3(header.hashPrevBlock);

    from.messages.clear();
    BOOST_CHECK(!p.requestConnectHeaders(header, from));
    BOOST_CHECK_EQUAL(0, from.messages.size());
    BOOST_CHECK_EQUAL(1 /* no change */, NodeStatePtr(from.id)->unconnectingHeaders);
}

BOOST_AUTO_TEST_SUITE_END();
