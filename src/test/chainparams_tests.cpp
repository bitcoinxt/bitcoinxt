// Copyright (c) 2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "chainparams.h"
#include "options.h"
#include "test/test_bitcoin.h"

#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(chainparams_tests, BasicTestingSetup);

BOOST_AUTO_TEST_CASE(check_network_magic) {
    auto cfg = new DummyArgGetter;
    auto raii = SetDummyArgGetter(std::unique_ptr<ArgGetter>(cfg));

    // Should return BTC magic.
    cfg->Set("-uahftime", 0);
    auto magic = Params(CBaseChainParams::MAIN).NetworkMagic();
    BOOST_CHECK_EQUAL(0xf9, magic[0]);

    // Should return BCH magic.
    cfg->Set("-uahftime", 42);
    magic = Params(CBaseChainParams::MAIN).NetworkMagic();
    BOOST_CHECK_EQUAL(0xe3, magic[0]);
}

BOOST_AUTO_TEST_SUITE_END();
