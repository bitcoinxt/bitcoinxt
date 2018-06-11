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

struct CVArgGetter : public ArgGetter {

    CVArgGetter() : hideplatform(false)
    {
    }

    bool GetBool(const std::string& arg, bool def) override {
        if (arg == "-hide-platform")
            return hideplatform;
        return def;
    }

    std::vector<std::string> GetMultiArgs(const std::string& arg) override {
        if (arg == "-uacomment")
            return uacomment;
        return std::vector<std::string>();
    }
    int64_t GetArg(const std::string& strArg, int64_t nDefault) override {
        assert(false);
    }
    std::string GetArg(const std::string& arg, const std::string& def) override
    {
        if (arg == "-useragent")
            return useragent;
        assert(false);
    }
    bool hideplatform;
    std::vector<std::string> uacomment;
    std::string useragent;
};

bool OsInStr(const std::string& version) {
    std::vector<std::string> systems = {
        "BSD", "Linux", "Mac OS", "Windows", "Unknown OS"
    };
    for (std::string os : systems)
        if (version.find(os) != std::string::npos)
            return true;
    return false;
}

BOOST_AUTO_TEST_CASE(platform_in_xtsubversion)
{
    auto argPtr = new CVArgGetter;
    auto argraii = SetDummyArgGetter(std::unique_ptr<ArgGetter>(argPtr));

    argPtr->hideplatform = true;
    argPtr->useragent = "";
    BOOST_CHECK(!OsInStr(XTSubVersion(0)));

    argPtr->hideplatform = false;
    argPtr->useragent = "/test:1.0/";
    BOOST_CHECK(!OsInStr(XTSubVersion(0)));

#if BOOST_VERSION >= 105500
    argPtr->hideplatform = false;
    argPtr->useragent = "";
    BOOST_CHECK(OsInStr(XTSubVersion(0)));
#endif
}

BOOST_AUTO_TEST_CASE(xtsubversion_stealthmode)
{
    auto argPtr = new CVArgGetter;
    auto argraii = SetDummyArgGetter(std::unique_ptr<ArgGetter>(argPtr));

    argPtr->useragent = "/test:1.0/";
    BOOST_CHECK(XTSubVersion(0).find("XT") == std::string::npos);

    argPtr->useragent = "";
    BOOST_CHECK(XTSubVersion(0).find("XT") != std::string::npos);
}

BOOST_AUTO_TEST_CASE(xtsubversion_uacomment)
{
    auto argPtr = new CVArgGetter;
    auto argraii = SetDummyArgGetter(std::unique_ptr<ArgGetter>(argPtr));

    argPtr->hideplatform = true;

    // only EB comment
    BOOST_CHECK(XTSubVersion(0).find("(EB0)") != std::string::npos);

    // uacomments + EB
    argPtr->uacomment = {"hello", "world" };
    BOOST_CHECK(XTSubVersion(0).find("(hello; world; EB0)") != std::string::npos);

#if BOOST_VERSION >= 105500
    // combines with platform
    argPtr->hideplatform = false;
    std::string withplatform = XTSubVersion(0);
    BOOST_CHECK(withplatform.find("(hello; world; ") != std::string::npos);
    BOOST_CHECK(OsInStr(withplatform.substr(withplatform.find("(hello; world; "))));
#endif

    // not supported with custom user agent
    argPtr->useragent = "/test:1.0/";
    BOOST_CHECK(XTSubVersion(0).find("(hello; world)") == std::string::npos);
}

BOOST_AUTO_TEST_CASE(xtsubversion_eb)
{
    // 1MB blocks
    BOOST_CHECK(XTSubVersion(1000000).find("EB1)") != std::string::npos);
    // 1GB blocks
    BOOST_CHECK(XTSubVersion(1000 * 1000000).find("EB1000)") != std::string::npos);
}

BOOST_AUTO_TEST_CASE(xtsubversion_customagent) {
    auto argPtr = new CVArgGetter;
    auto argraii = SetDummyArgGetter(std::unique_ptr<ArgGetter>(argPtr));

    argPtr->useragent = "/hello world:0.1/";
    BOOST_CHECK_EQUAL("/hello world:0.1/", XTSubVersion(1000000));
}

BOOST_AUTO_TEST_SUITE_END()
