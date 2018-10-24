// Copyright (c) 2018 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "test/test_bitcoin.h"
#include "script/sighashtype.h"

#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(sighashtype_tests, BasicTestingSetup);

BOOST_AUTO_TEST_CASE(bitwise_tests) {
    BOOST_CHECK_EQUAL(0x1 | 0x2, ToInt(SigHashType(0x1) | SigHashType(0x2)));
    BOOST_CHECK_EQUAL(0x1 & 0x3, ToInt(SigHashType(0x1) & SigHashType(0x3)));
    BOOST_CHECK_EQUAL(0x3 & ~0x1, ToInt(SigHashType(0x3) & ~SigHashType(0x1)));
    BOOST_CHECK_EQUAL(0xC0, ToInt(SigHashType(0xC3) & ~SigHashType(0x3)));

    SigHashType t = SigHashType(0x1);
    t |= SigHashType(0x2);
    BOOST_CHECK_EQUAL(SigHashType(0x3), t);
    t &= SigHashType(0x1);
    BOOST_CHECK_EQUAL(SigHashType(0x1), t);
}

BOOST_AUTO_TEST_CASE(boolean_tests) {
    BOOST_CHECK(!bool(SigHashType(0x0)));
    BOOST_CHECK(bool(SigHashType(0x3)));
    BOOST_CHECK(!SigHashType(0x0) == true);
    BOOST_CHECK(!SigHashType(0x3) == false);
}

BOOST_AUTO_TEST_CASE(basetype_tests) {
    SigHashType manyflags = SigHashType::SINGLE | SigHashType::FORKID | SigHashType::ANYONECANPAY;

    BOOST_CHECK_EQUAL(SigHashType::SINGLE, GetBaseType(manyflags));
    BOOST_CHECK_EQUAL(SigHashType::FORKID | SigHashType::ANYONECANPAY,
                      RemoveBaseType(manyflags));
}

BOOST_AUTO_TEST_CASE(strconversion) {
    BOOST_CHECK_EQUAL(SigHashType::ALL | SigHashType::ANYONECANPAY,
                      FromStr("ALL|ANYONECANPAY"));
    BOOST_CHECK_EQUAL("ALL|ANYONECANPAY",
                      ToStr(SigHashType::ALL | SigHashType::ANYONECANPAY));
    // unsupported
    BOOST_CHECK_THROW(FromStr("SINGLE|NONE"), std::invalid_argument);
    BOOST_CHECK_THROW(ToStr(SigHashType(0x30)), std::invalid_argument);
}

BOOST_AUTO_TEST_CASE(intconverison) {
    BOOST_CHECK_EQUAL(0x30, ToInt(SigHashType(0x30)));
}

BOOST_AUTO_TEST_SUITE_END();
