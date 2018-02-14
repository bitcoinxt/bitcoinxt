// Copyright (c) 2016 The Bitcoin XT developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <boost/test/unit_test.hpp>

#include "test/test_bitcoin.h"
#include "options.h"

#include <limits.h>

const int NOT_SET = std::numeric_limits<int>::min();

class DummyArgGetter : public ArgGetter {
    public:

    DummyArgGetter() : ArgGetter(),
                       par(NOT_SET), checkpdays(NOT_SET), uahftime(NOT_SET)
    {
    }

    virtual int64_t GetArg(const std::string& strArg, int64_t nDefault) {
        if (strArg == "-par")
            return par == NOT_SET ? nDefault : par;

        if (strArg == "-checkpoint-days")
            return checkpdays == NOT_SET ? nDefault : checkpdays;

        if (strArg == "-uahftime" && uahftime != NOT_SET)
            return uahftime;

        return nDefault;
    }
    virtual std::vector<std::string> GetMultiArgs(const std::string& arg) {
        assert(false);
    }
    virtual bool GetBool(const std::string& arg, bool def) {
        assert(false);
    }
    int par;
    int checkpdays;
    int64_t uahftime;
};

BOOST_FIXTURE_TEST_SUITE(options_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(scripthreads) {
    std::unique_ptr<DummyArgGetter> arg(new DummyArgGetter);
    DummyArgGetter* argPtr = arg.get();
    std::unique_ptr<ArgReset> argraii
        = SetDummyArgGetter(std::unique_ptr<ArgGetter>(arg.release()));

    argPtr->par = 1; // not threaded
    BOOST_CHECK_EQUAL(0, Opt().ScriptCheckThreads());

    argPtr->par = 3; // 3 threads
    BOOST_CHECK_EQUAL(3, Opt().ScriptCheckThreads());

    // auto case not tested
}

BOOST_AUTO_TEST_CASE(checkpointdays) {
    std::unique_ptr<DummyArgGetter> arg(new DummyArgGetter);
    DummyArgGetter* argPtr = arg.get();
    std::unique_ptr<ArgReset> argraii
        = SetDummyArgGetter(std::unique_ptr<ArgGetter>(arg.release()));

    // No multiplier
    argPtr->par = 1;
    BOOST_CHECK_EQUAL(DEFAULT_CHECKPOINT_DAYS, Opt().CheckpointDays());

    // x3 multiplier (3 script threads)
    argPtr->par = 3;
    BOOST_CHECK_EQUAL(3 * DEFAULT_CHECKPOINT_DAYS, Opt().CheckpointDays());

    // Explicitly set days overrides default and multipliers
    argPtr->checkpdays = 1;
    BOOST_CHECK_EQUAL(1, Opt().CheckpointDays());

     // Can't have less than 1 day
    argPtr->checkpdays = 0;
    BOOST_CHECK_EQUAL(1, Opt().CheckpointDays());
}

BOOST_AUTO_TEST_CASE(may2018hftime_ignored_for_btc) {
    auto arg = new DummyArgGetter;
    auto argraii = SetDummyArgGetter(std::unique_ptr<ArgGetter>(arg));

    // Enabled by default
    BOOST_CHECK_EQUAL(1526400000, Opt().May2018HFTime());

    // Disabled if we're not on Bitcoin Cash chain
    arg->uahftime = 0;
    BOOST_CHECK_EQUAL(0, Opt().May2018HFTime());
}

BOOST_AUTO_TEST_SUITE_END()
