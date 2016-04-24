// Copyright (c) 2016- The Bitcoin XT developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#include <boost/test/unit_test.hpp>
#include "net.h"
#include "ipgroups.h"

BOOST_AUTO_TEST_SUITE(node_tests);

BOOST_AUTO_TEST_CASE(support_xthin_test) {
    CNode node(INVALID_SOCKET, CAddress());
    node.id = 42;

    BOOST_CHECK(!node.SupportsXThinBlocks());
    node.nServices |= NODE_THIN;
    BOOST_CHECK(node.SupportsXThinBlocks());
}

// Test that a correct IP group is created for node, then removed
// when node is destructed.
BOOST_AUTO_TEST_CASE(ipgroup_assigned) {
    CNetAddr ip("10.0.0.1");
    std::auto_ptr<CNode> node(new CNode(
                INVALID_SOCKET, CAddress(CService(ip, 1234))));

    CIPGroupData ipgroup = FindGroupForIP(ip);
    BOOST_CHECK_EQUAL(1, ipgroup.connCount);
    BOOST_CHECK_EQUAL(ip.ToStringIP(), ipgroup.name);

    node.reset();
    ipgroup = FindGroupForIP(ip);
    BOOST_CHECK_EQUAL(0, ipgroup.connCount);

}

BOOST_AUTO_TEST_SUITE_END();
