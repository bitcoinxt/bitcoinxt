// Copyright (c) 2016 The Bitcoin XT developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <boost/test/unit_test.hpp>

#include "options.h"

struct DummyArgGetter : public ArgGetter {
    virtual int64_t GetArg(const std::string& strArg, int64_t nDefault) {
        assert(strArg == "-par");
        return par;
    }
    virtual std::vector<std::string> GetMultiArgs(const std::string& arg) {
        assert(false);
    }
    virtual bool GetBool(const std::string& arg, bool def) {
        assert(false);
    }
    int par;
};

BOOST_AUTO_TEST_SUITE(options_tests);

BOOST_AUTO_TEST_CASE(scripthreads) {
    std::auto_ptr<DummyArgGetter> arg(new DummyArgGetter);
    DummyArgGetter* argPtr = arg.get();
    std::auto_ptr<ArgReset> argraii
        = SetDummyArgGetter(std::auto_ptr<ArgGetter>(arg.release()));

    argPtr->par = 0; // auto
    BOOST_CHECK(Opt().ScriptCheckThreads() > 0);

    argPtr->par = 1; // not threaded
    BOOST_CHECK_EQUAL(0, Opt().ScriptCheckThreads());

    argPtr->par = 3; // 3 threads
    BOOST_CHECK_EQUAL(3, Opt().ScriptCheckThreads());
}

BOOST_AUTO_TEST_SUITE_END()
