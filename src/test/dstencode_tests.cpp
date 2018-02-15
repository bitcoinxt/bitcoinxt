// Copyright (c) 2017 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "chainparams.h"
#include "dstencode.h"
#include "options.h"
#include "test/test_bitcoin.h"

#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(dstencode_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(test_addresses) {
    std::vector<uint8_t> hash = {118, 160, 64,  83,  189, 160, 168,
                                 139, 218, 81,  119, 184, 106, 21,
                                 195, 178, 159, 85,  152, 115};

    const CTxDestination dstKey = CKeyID(uint160(hash));
    const CTxDestination dstScript = CScriptID(uint160(hash));

    std::string cashaddr_pubkey =
        "bitcoincash:qpm2qsznhks23z7629mms6s4cwef74vcwvy22gdx6a";
    std::string cashaddr_script =
        "bitcoincash:ppm2qsznhks23z7629mms6s4cwef74vcwvn0h829pq";
    std::string base58_pubkey = "1BpEi6DfDAUFd7GtittLSdBeYJvcoaVggu";
    std::string base58_script = "3CWFddi6m4ndiGyKqzYvsFYagqDLPVMTzC";

    const CChainParams &params = Params(CBaseChainParams::MAIN);
    auto arg = new DummyArgGetter;
    auto raii = SetDummyArgGetter(std::unique_ptr<ArgGetter>(arg));

    // Check encoding
    arg->Set("-usecashaddr", 1);
    BOOST_CHECK_EQUAL(cashaddr_pubkey, EncodeDestination(dstKey, params));
    BOOST_CHECK_EQUAL(cashaddr_script, EncodeDestination(dstScript, params));
    arg->Set("-usecashaddr", 0);
    BOOST_CHECK_EQUAL(base58_pubkey, EncodeDestination(dstKey, params));
    BOOST_CHECK_EQUAL(base58_script, EncodeDestination(dstScript, params));

    // Check decoding
    BOOST_CHECK(dstKey == DecodeDestination(cashaddr_pubkey, params));
    BOOST_CHECK(dstScript == DecodeDestination(cashaddr_script, params));
    BOOST_CHECK(dstKey == DecodeDestination(base58_pubkey, params));
    BOOST_CHECK(dstScript == DecodeDestination(base58_script, params));

    // Validation
    BOOST_CHECK(IsValidDestinationString(cashaddr_pubkey, params));
    BOOST_CHECK(IsValidDestinationString(cashaddr_script, params));
    BOOST_CHECK(IsValidDestinationString(base58_pubkey, params));
    BOOST_CHECK(IsValidDestinationString(base58_script, params));
    BOOST_CHECK(!IsValidDestinationString("notvalid", params));
}

BOOST_AUTO_TEST_SUITE_END()
