// Copyright (c) 2015- The Bitcoin XT developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <boost/test/unit_test.hpp>

#include "clientversion.h"
#include "options.h"

#include <boost/version.hpp>
#if BOOST_VERSION >= 105500 // Boost 1.55 or newer
#include <boost/predef.h>
#endif

BOOST_AUTO_TEST_SUITE(clientversion_tests)

struct DummyArgGetter : public ArgGetter {

    DummyArgGetter() : stealthmode(false), hideplatform(false)
    {
    }

    virtual bool GetBool(const std::string& arg, bool def) {
        if (arg == "-stealth-mode")
            return stealthmode;
        if (arg == "-hide-platform")
            return hideplatform;
        return def;
    }

    virtual std::vector<std::string> GetMultiArgs(const std::string& arg) {
        if (arg == "-uacomment")
            return uacomment;
        return std::vector<std::string>();
    }
    virtual int64_t GetArg(const std::string& strArg, int64_t nDefault) {
        assert(false);
    }


    bool stealthmode;
    bool hideplatform;
    std::vector<std::string> uacomment;
};

bool OsInStr(const std::string& version) {
    // Assume OS is in string if string contains comments
    return version.find("(") != std::string::npos;
}

BOOST_AUTO_TEST_CASE(platform_in_xtsubversion)
{
    std::unique_ptr<DummyArgGetter> arg(new DummyArgGetter);
    DummyArgGetter* argPtr = arg.get();
    std::unique_ptr<ArgReset> argraii
        = SetDummyArgGetter(std::unique_ptr<ArgGetter>(arg.release()));

    argPtr->hideplatform = true;
    argPtr->stealthmode = false;
    BOOST_CHECK(!OsInStr(XTSubVersion()));

    argPtr->hideplatform = false;
    argPtr->stealthmode = true;
    BOOST_CHECK(!OsInStr(XTSubVersion()));

#if BOOST_VERSION >= 105500
    argPtr->hideplatform = false;
    argPtr->stealthmode = false;
    BOOST_CHECK(OsInStr(XTSubVersion()));
#endif
}

BOOST_AUTO_TEST_CASE(xtsubversion_stealthmode)
{
    std::unique_ptr<DummyArgGetter> arg(new DummyArgGetter);
    DummyArgGetter* argPtr = arg.get();
    std::unique_ptr<ArgReset> argraii
        = SetDummyArgGetter(std::unique_ptr<ArgGetter>(arg.release()));

    argPtr->stealthmode = true;
    BOOST_CHECK(XTSubVersion().find("XT") == std::string::npos);

    argPtr->stealthmode = false;
    BOOST_CHECK(XTSubVersion().find("XT") != std::string::npos);
}

BOOST_AUTO_TEST_CASE(xtsubversion_uacomment)
{
    std::unique_ptr<DummyArgGetter> arg(new DummyArgGetter);
    DummyArgGetter* argPtr = arg.get();
    std::unique_ptr<ArgReset> argraii
        = SetDummyArgGetter(std::unique_ptr<ArgGetter>(arg.release()));

    argPtr->hideplatform = true;

    // no comments
    BOOST_CHECK(XTSubVersion().find("(") == std::string::npos);

    // only uacomments
    argPtr->uacomment = {"hello", "world" };
    BOOST_CHECK(XTSubVersion().find("(hello; world)") != std::string::npos);

#if BOOST_VERSION >= 105500
    // combines with platform
    argPtr->hideplatform = false;
    BOOST_CHECK(XTSubVersion().find("(hello; world; ") != std::string::npos);
#endif

    // allowed in stealth-mode
    argPtr->stealthmode = true;
    BOOST_CHECK(XTSubVersion().find("(hello; world)") != std::string::npos);
}

BOOST_AUTO_TEST_SUITE_END()

