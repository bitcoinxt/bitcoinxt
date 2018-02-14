// Copyright (c) 2017 - 2018 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#include "chain.h"
#include "chainparams.h"
#include "options.h"
#include "test/test_bitcoin.h"
#include "utilfork.h"

#include <limits.h>
#include <iostream>

#include <boost/test/unit_test.hpp>

namespace {
class DummyArgGetter : public ArgGetter {
    public:
    DummyArgGetter() : ArgGetter(), uahf(-1), mayhf(-1) { }

    int64_t GetArg(const std::string& arg, int64_t def) override {
        if (arg == "-uahftime")
            return uahf;
        if (arg == "-may2018hftime")
            return mayhf;
        throw std::runtime_error("not implemented");
    }
    std::vector<std::string> GetMultiArgs(const std::string&) override {
        throw std::runtime_error("not implemented");
    }
    bool GetBool(const std::string&, bool) override {
        throw std::runtime_error("not implemented");
    }
    int64_t uahf;
    int64_t mayhf;
};

bool isActivating(const CBlockIndex& b) {
    return IsUAHFActivatingBlock(b.GetMedianTimePast(), b.pprev);
}

} // ns anon

BOOST_FIXTURE_TEST_SUITE(utilfork_tests, BasicTestingSetup);

BOOST_AUTO_TEST_CASE(is_uahf_activating_block_at_genesis) {

    auto cfgPtr = new DummyArgGetter;
    DummyArgGetter& cfg = *cfgPtr;
    auto argraii = SetDummyArgGetter(std::unique_ptr<ArgGetter>(cfgPtr));

    CBlockIndex genesis;
    genesis.pprev = nullptr;
    genesis.nTime = 11;

    // UAHF activates after genesis
    cfg.uahf = genesis.GetMedianTimePast() + 1;
    BOOST_CHECK(!isActivating(genesis));

    // UAHF activates on genesis
    cfg.uahf = genesis.GetMedianTimePast();
    BOOST_CHECK(isActivating(genesis));

    // UAHF is disabled.
    cfg.uahf = 0;
    BOOST_CHECK(!isActivating(genesis));
}

BOOST_AUTO_TEST_CASE(is_uahf_activating_block_normal) {
    // normal case, activation is somewhere in the chain (not at genesis)

    auto cfgPtr = new DummyArgGetter;
    DummyArgGetter& cfg = *cfgPtr;
    auto argraii = SetDummyArgGetter(std::unique_ptr<ArgGetter>(cfgPtr));

    CBlockIndex genesis;
    genesis.pprev = nullptr;
    genesis.nTime = 11;

    CBlockIndex block2;
    block2.pprev = &genesis;
    block2.nTime = 42;

    CBlockIndex block3;
    block3.pprev = &block2;
    block3.nTime = 50;

    CBlockIndex block4;
    block4.pprev = &block3;
    block4.nTime = 100;

    // Activation time is exactly mtp of block2.
    // In this test block2 and block3 have the same mtp.
    cfg.uahf = block2.GetMedianTimePast();

    BOOST_CHECK(!isActivating(genesis));
    BOOST_CHECK(isActivating(block2));
    BOOST_CHECK(!isActivating(block3));
    BOOST_CHECK(!isActivating(block4));

    // Activation time is before mtp of block2 (but after mtp of genesis)
    cfg.uahf = block2.GetMedianTimePast() - 1;
    BOOST_CHECK(!isActivating(genesis));
    BOOST_CHECK(isActivating(block2));
    BOOST_CHECK(!isActivating(block3));
    BOOST_CHECK(!isActivating(block4));

    // No activation yet.
    cfg.uahf = block4.GetMedianTimePast() + 1;
    BOOST_CHECK(!isActivating(genesis));
    BOOST_CHECK(!isActivating(block2));
    BOOST_CHECK(!isActivating(block3));
    BOOST_CHECK(!isActivating(block4));
}

BOOST_AUTO_TEST_CASE(is_may2018hf_active) {
    auto arg = new DummyArgGetter;
    auto argraii = SetDummyArgGetter(std::unique_ptr<ArgGetter>(arg));

    CBlockIndex genesis;
    genesis.pprev = nullptr;
    genesis.nTime = 11;

    CBlockIndex block2;
    block2.pprev = &genesis;
    block2.nTime = 42;

    CBlockIndex block3;
    block3.pprev = &block2;
    block3.nTime = 50;

    CBlockIndex block4;
    block4.pprev = &block3;
    block4.nTime = 100;

    arg->uahf = 1;
    // Activation time is exactly mtp of block2.
    // In this test block2 and block3 have the same mtp.
    arg->mayhf = block2.GetMedianTimePast();

    BOOST_CHECK(!IsMay2018HFActive(genesis.GetMedianTimePast()));
    BOOST_CHECK(IsMay2018HFActive(block2.GetMedianTimePast()));
    BOOST_CHECK(IsMay2018HFActive(block3.GetMedianTimePast()));
    BOOST_CHECK(IsMay2018HFActive(block4.GetMedianTimePast()));

    // Never active if mayhf is disabled.
    arg->mayhf = 0;
    BOOST_CHECK(!IsMay2018HFActive(genesis.GetMedianTimePast()));
    BOOST_CHECK(!IsMay2018HFActive(block2.GetMedianTimePast()));
    BOOST_CHECK(!IsMay2018HFActive(block3.GetMedianTimePast()));
    BOOST_CHECK(!IsMay2018HFActive(block4.GetMedianTimePast()));
}

BOOST_AUTO_TEST_SUITE_END()
