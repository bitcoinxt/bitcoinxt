// Copyright (c) 2016 The Bitcoin XT developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <boost/test/unit_test.hpp>

#include "test/test_bitcoin.h"
#include "options.h"

#include <limits.h>

BOOST_AUTO_TEST_SUITE(options_tests);

BOOST_AUTO_TEST_CASE(scripthreads) {
    auto arg = new DummyArgGetter;
    auto argraii = SetDummyArgGetter(std::unique_ptr<ArgGetter>(arg));

    // not threaded
    arg->Set("-par", 1);
    BOOST_CHECK_EQUAL(0, Opt().ScriptCheckThreads());

    // 3 threads
    arg->Set("-par", 3);
    BOOST_CHECK_EQUAL(3, Opt().ScriptCheckThreads());

    // auto case not tested
}

BOOST_AUTO_TEST_CASE(checkpointdays) {
    auto arg = new DummyArgGetter;
    auto argraii = SetDummyArgGetter(std::unique_ptr<ArgGetter>(arg));

    // No multiplier
    arg->Set("-par", 1);
    BOOST_CHECK_EQUAL(DEFAULT_CHECKPOINT_DAYS, Opt().CheckpointDays());

    // x3 multiplier (3 script threads)
    arg->Set("-par", 3);
    BOOST_CHECK_EQUAL(3 * DEFAULT_CHECKPOINT_DAYS, Opt().CheckpointDays());

    // Explicitly set days overrides default and multipliers
    arg->Set("-checkpoint-days", 1);
    BOOST_CHECK_EQUAL(1, Opt().CheckpointDays());

     // Can't have less than 1 day
    arg->Set("-checkpoint-days", 0);
    BOOST_CHECK_EQUAL(1, Opt().CheckpointDays());
}

BOOST_AUTO_TEST_CASE(thirdhftime_ignored_for_btc) {
    auto arg = new DummyArgGetter;
    auto argraii = SetDummyArgGetter(std::unique_ptr<ArgGetter>(arg));

    // Enabled by default
    BOOST_CHECK_EQUAL(1526400000, Opt().ThirdHFTime());

    // Disabled if we're not on Bitcoin Cash chain
    arg->Set("-uahftime", 0);
    BOOST_CHECK_EQUAL(0, Opt().ThirdHFTime());
}

BOOST_AUTO_TEST_CASE(fourthhftime_ignored_for_btc) {
    auto arg = new DummyArgGetter;
    auto argraii = SetDummyArgGetter(std::unique_ptr<ArgGetter>(arg));

    // Enabled by default
    arg->Set("-fourthhftime", 1542300000); // TODO: REVERT ME!
    BOOST_CHECK_EQUAL(1542300000, Opt().FourthHFTime());

    // Disabled if we're not on Bitcoin Cash chain
    arg->Set("-uahftime", 0);
    BOOST_CHECK_EQUAL(0, Opt().FourthHFTime());
}

BOOST_AUTO_TEST_SUITE_END()
