// Copyright (c) 2016- The Bitcoin XT developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#include <boost/test/unit_test.hpp>
#include "net.h"

BOOST_AUTO_TEST_SUITE(node_tests);

BOOST_AUTO_TEST_CASE(support_xthin_test) {
    CNode node(INVALID_SOCKET, CAddress());
    node.id = 42;

    BOOST_CHECK(!node.SupportsXThinBlocks());
    node.nServices |= NODE_THIN;
    BOOST_CHECK(node.SupportsXThinBlocks());
}

BOOST_AUTO_TEST_SUITE_END();
