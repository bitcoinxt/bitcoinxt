// Copyright (c) 2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <boost/test/unit_test.hpp>
#include "chainparams.h"
#include "options.h"

namespace {

class OptDummy : public ArgGetter {
    public:
        OptDummy() : uahftime(0) { }
        int64_t GetArg(const std::string& arg, int64_t def) override {
            return arg == "-uahftime" ? uahftime : def;
        }

        bool GetBool(const std::string&, bool def) override
        { return def; }

        std::vector<std::string> GetMultiArgs(const std::string&) override
        { return { }; }

        int64_t uahftime;
};

} // anon ns

BOOST_AUTO_TEST_SUITE(chainparams_tests);

BOOST_AUTO_TEST_CASE(check_network_magic) {
    auto cfg = new OptDummy();
    auto raii = SetDummyArgGetter(std::unique_ptr<ArgGetter>(cfg));

    // Should return BTC magic.
    cfg->uahftime = 0;
    auto magic = Params(CBaseChainParams::MAIN).NetworkMagic();
    BOOST_CHECK_EQUAL(0xf9, magic[0]);

    // Should return BCH magic.
    cfg->uahftime = 42;
    magic = Params(CBaseChainParams::MAIN).NetworkMagic();
    BOOST_CHECK_EQUAL(0xe3, magic[0]);
}

BOOST_AUTO_TEST_SUITE_END();
