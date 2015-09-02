// Copyright (c) 2015- The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/test/unit_test_suite.hpp>
#include <boost/test/test_tools.hpp>

#include <ipgroups.h>
#include <util.h>
#include <test/test_bitcoin.h>

extern std::vector<CSubNet> ParseIPData(std::string input);

BOOST_AUTO_TEST_SUITE(ipgroups_tests);

BOOST_AUTO_TEST_CASE(ipgroup)
{
    InitIPGroups(NULL);
    BOOST_CHECK_EQUAL(FindGroupForIP(CNetAddr("0.0.0.0")).priority, 0);
    BOOST_CHECK_EQUAL(FindGroupForIP(CNetAddr("0.1.2.3")).name, "tor_static");

    // If/when we have the ability to load labelled subnets from a file or persisted datastore, test that here.
}

BOOST_AUTO_TEST_CASE(parse_ip_data_ok)
{
    std::string data =
            "# You can update this list by visiting https://check.torproject.org/cgi-bin/TorBulkExitList.py?ip=185.25.95.132&port=8333 #\n"
            "# This file was generated on Thu Aug 27 14:04:18 UTC 2015 #\n"
            "1.160.81.87\n"
            "1.160.87.94\n"
            "1.2.3.4/24";

    std::vector<CSubNet> subnets = ParseIPData(data);
    BOOST_CHECK(subnets[0] == CSubNet("1.160.81.87/32"));
    BOOST_CHECK(subnets[1] == CSubNet("1.160.87.94/32"));
    BOOST_CHECK(subnets[2] == CSubNet("1.2.3.4/24"));
}

BOOST_AUTO_TEST_CASE(parse_ip_data_bad)
{
    BOOST_CHECK_EQUAL(ParseIPData("").size(), 0);
    BOOST_CHECK_EQUAL(ParseIPData("Blah blah blah\t\t").size(), 0);
    BOOST_CHECK_EQUAL(ParseIPData("   #").size(), 0);
    BOOST_CHECK_EQUAL(ParseIPData("256.0.0.0").size(), 0);
    BOOST_CHECK_EQUAL(ParseIPData("-1.0.0.0").size(), 0);
    BOOST_CHECK_EQUAL(ParseIPData("1.2.").size(), 0);

    // These tests reveal that the CSubNet parser can sometimes fail to spot that a subnet specification is invalid.
    // If/when CSubNet("") is tightened up we can have more aggressive testing of junk here.
    // BOOST_CHECK_EQUAL(ParseIPData("1.2.3")[0].ToString(), "1.2.3.0/24");
    // BOOST_CHECK_EQUAL(ParseIPData("[2a00:1450:400a:805::").size(), 0);   // truncated
}

BOOST_AUTO_TEST_CASE(cmd_line_sources)
{
    TestingSetup setup;

    {
        boost::filesystem::ofstream stream(GetDataDir() / "ips-test1.txt");
        stream << "100.200.3.4/24";
    }
    {
        boost::filesystem::ofstream stream(GetDataDir() / "ips-test2.txt");
        stream << "100.200.5.6";
    }
    mapMultiArgs[IP_PRIO_SRC_FLAG_NAME].push_back("test1,10,ips-test1.txt");
    mapMultiArgs[IP_PRIO_SRC_FLAG_NAME].push_back("test2,-10,ips-test2.txt");
    InitIPGroupsFromCommandLine();

    BOOST_CHECK_EQUAL(FindGroupForIP(CNetAddr("100.200.3.1")).priority, 10);
    BOOST_CHECK_EQUAL(FindGroupForIP(CNetAddr("100.200.3.1")).name, "test1");

    BOOST_CHECK_EQUAL(FindGroupForIP(CNetAddr("100.200.5.6")).priority, -10);
    BOOST_CHECK_EQUAL(FindGroupForIP(CNetAddr("100.200.5.6")).name, "test2");

    // Check we're OK if the file disappears.
    boost::filesystem::remove(GetDataDir() / "ips-test2.txt");
    InitIPGroupsFromCommandLine();
    BOOST_CHECK_EQUAL(FindGroupForIP(CNetAddr("100.200.5.6")).priority, 0);
    BOOST_CHECK_EQUAL(FindGroupForIP(CNetAddr("100.200.3.1")).priority, 10);

    // Or gets corrupted ....
    boost::filesystem::resize_file(GetDataDir() / "ips-test1.txt", 3);
    InitIPGroupsFromCommandLine();
    BOOST_CHECK_EQUAL(FindGroupForIP(CNetAddr("100.200.3.1")).priority, 0);
}

BOOST_AUTO_TEST_SUITE_END()

