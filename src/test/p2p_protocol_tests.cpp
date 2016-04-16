// Copyright (c) 2011-2015 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "main.h"

#include "test/test_bitcoin.h"

#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(p2p_protocol_tests, TestingSetup)

BOOST_AUTO_TEST_CASE(MaxSizeVersionMessage)
{
    CNode n(INVALID_SOCKET, CAddress(CService("127.0.0.1", 0), NODE_NETWORK));
    CDataStream s(SER_NETWORK, PROTOCOL_VERSION);
    nLocalHostNonce = 2; // this trips the version message logic to end shortly after reading the data (which is the focus of this test)
    s << PROTOCOL_VERSION;
    s << n.nServices;
    s << GetTime();
    s << CAddress(CService("0.0.0.0", 0));
    s << CAddress(CService("0.0.0.0", 0));
    s << nLocalHostNonce;
    s << std::string(256, 'a'); // 256 is the max allowed length in the Bitcoin Core/XT protocol processing code
    s << n.nStartingHeight;
    s << n.fRelayTxes;
    BOOST_CHECK_EQUAL(size_t(352), s.size());
    CConnman dummy;
    BOOST_CHECK(ProcessMessage(&n, "version", s, 0, &dummy));
}

BOOST_AUTO_TEST_CASE(OverMaxSizeVersionMessage)
{
    CNode n(INVALID_SOCKET, CAddress(CService("127.0.0.1", 0), NODE_NETWORK));
    CDataStream s(SER_NETWORK, PROTOCOL_VERSION);
    nLocalHostNonce = 2; // this trips the version message logic to end shortly after reading the data (which is the focus of this test)
    s << PROTOCOL_VERSION;
    s << n.nServices;
    s << GetTime();
    s << CAddress(CService("0.0.0.0", 0));
    s << CAddress(CService("0.0.0.0", 0));
    s << nLocalHostNonce;
    s << std::string(257, 'a'); // invalid, max is 256
    s << n.nStartingHeight;
    s << n.fRelayTxes;
    BOOST_CHECK_EQUAL(size_t(353), s.size());
    CConnman dummy;
    BOOST_CHECK_THROW(ProcessMessage(&n, "version", s, 0, &dummy), std::ios_base::failure);
}

BOOST_AUTO_TEST_CASE(MaxSizeWeirdRejectMessage)
{
    CNode n(INVALID_SOCKET, CAddress(CService("127.0.0.1", 0), NODE_NETWORK));
    n.nVersion = PROTOCOL_VERSION;
    CDataStream s(SER_NETWORK, PROTOCOL_VERSION);
    s << std::string(12, 'a'); // not a real command, but it uses the max of 12 here.
    s << (uint8_t)0x10;
    s << std::string(111, 'a');
    BOOST_CHECK_EQUAL(size_t(126), s.size());
    bool temp = fDebug;
    fDebug = true;
    CConnman dummy;
    BOOST_CHECK(ProcessMessage(&n, "reject", s, 0, &dummy));
    fDebug = temp;
}

BOOST_AUTO_TEST_CASE(MaxSizeValidRejectMessage)
{
    CNode n(INVALID_SOCKET, CAddress(CService("127.0.0.1", 0), NODE_NETWORK));
    n.nVersion = PROTOCOL_VERSION;
    CDataStream s(SER_NETWORK, PROTOCOL_VERSION);
    s << std::string("block"); // does not use the max of 12, but "block" is the longest command that has a defined extension of 32 bytes
    s << (uint8_t)0x10;
    s << std::string(111, 'a');
    s << uint256();
    BOOST_CHECK_EQUAL(size_t(151), s.size());
    bool temp = fDebug;
    fDebug = true;
    CConnman dummy;
    BOOST_CHECK(ProcessMessage(&n, "reject", s, 0, &dummy));
    fDebug = temp;
}

BOOST_AUTO_TEST_CASE(OverMaxSizeWeirdRejectMessage)
{
    CNode n(INVALID_SOCKET, CAddress(CService("127.0.0.1", 0), NODE_NETWORK));
    n.nVersion = PROTOCOL_VERSION;
    CDataStream s(SER_NETWORK, PROTOCOL_VERSION);
    s << std::string(13, 'a'); // invalid, max is 12
    s << (uint8_t)0x10;
    s << std::string(111, 'a');
    BOOST_CHECK_EQUAL(size_t(127), s.size());
    bool temp = fDebug;
    fDebug = true;
    CConnman dummy;
    BOOST_CHECK(!ProcessMessage(&n, "reject", s, 0, &dummy)); // check this way since the reject message processing swallows the exception
    fDebug = temp;
}

BOOST_AUTO_TEST_CASE(OverMaxSizeValidRejectMessage)
{
    CNode n(INVALID_SOCKET, CAddress(CService("127.0.0.1", 0), NODE_NETWORK));
    n.nVersion = PROTOCOL_VERSION;
    CDataStream s(SER_NETWORK, PROTOCOL_VERSION);
    s << std::string("block");
    s << (uint8_t)0x10;
    s << std::string(112, 'a'); // invalid, max is 111
    s << uint256();
    BOOST_CHECK_EQUAL(size_t(152), s.size());
    bool temp = fDebug;
    fDebug = true;
    CConnman dummy;
    BOOST_CHECK(!ProcessMessage(&n, "reject", s, 0, &dummy)); // check this way since the reject message processing swallows the exception
    fDebug = temp;
}

BOOST_AUTO_TEST_SUITE_END()
