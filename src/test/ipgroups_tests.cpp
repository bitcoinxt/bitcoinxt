// Copyright (c) 2015- The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <ipgroups.h>
#include <boost/test/unit_test_suite.hpp>
#include <boost/test/test_tools.hpp>

extern std::vector<CSubNet> ParseTorData(std::string input);

BOOST_AUTO_TEST_SUITE(ipgroups_tests);

BOOST_AUTO_TEST_CASE(ipgroup)
{
    InitIPGroups(NULL);
    BOOST_CHECK_EQUAL(FindGroupForIP(CNetAddr("0.0.0.0")).priority, 0);
    BOOST_CHECK_EQUAL(FindGroupForIP(CNetAddr("0.1.2.3")).name, "tor_static");

    // If/when we have the ability to load labelled subnets from a file or persisted datastore, test that here.
}

BOOST_AUTO_TEST_CASE(parse_tor_data_ok)
{
    std::string data =
            "ExitNode 0011BD2485AD45D984EC4159C88FC066E5E3300E\n"
            "Published 2015-07-22 10:12:31\n"
            "LastStatus 2015-07-22 11:02:47\n"
            "ExitAddress 162.247.72.201 2015-07-22 11:09:35\n"
            "ExitNode 0098C475875ABC4AA864738B1D1079F711C38287\n"
            "Published 2015-07-22 02:00:35\n"
            "LastStatus 2015-07-22 03:02:56\n"
            "ExitAddress 162.248.160.151 2015-07-22 03:03:36";

    std::vector<CSubNet> subnets = ParseTorData(data);
    BOOST_CHECK(subnets[0] == CSubNet("162.247.72.201/32"));
    BOOST_CHECK(subnets[1] == CSubNet("162.248.160.151/32"));

    BOOST_CHECK_EQUAL(ParseTorData("ExitAddress [2a00:1450:400a:805::1007]")[0].ToString(), "2a00:1450:400a:805::1007/ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff");
}

BOOST_AUTO_TEST_CASE(parse_tor_data_bad)
{
    BOOST_CHECK_EQUAL(ParseTorData("").size(), 0);
    BOOST_CHECK_EQUAL(ParseTorData("Blah blah blah\t\t").size(), 0);
    BOOST_CHECK_EQUAL(ParseTorData("ExitAddress").size(), 0);
    BOOST_CHECK_EQUAL(ParseTorData("ExitAddress 256.0.0.0").size(), 0);
    BOOST_CHECK_EQUAL(ParseTorData("ExitAddress -1.0.0.0").size(), 0);
    BOOST_CHECK_EQUAL(ParseTorData("ExitAddress 1.2.3.4/24").size(), 0);
    BOOST_CHECK_EQUAL(ParseTorData("ExitAddress 1.2.").size(), 0);

    // These tests reveal that the CSubNet parser can sometimes fail to spot that a subnet specification is invalid.
    // If/when CSubNet("") is tightened up we can have more aggressive testing of junk here.
    // BOOST_CHECK_EQUAL(ParseTorData("ExitAddress 1.2.3")[0].ToString(), "1.2.3.0/24");
    // BOOST_CHECK_EQUAL(ParseTorData("ExitAddress [2a00:1450:400a:805::").size(), 0);   // truncated
}

BOOST_AUTO_TEST_SUITE_END()

