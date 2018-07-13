// Copyright (c) 2015- The Bitcoin XT developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <boost/test/unit_test.hpp>

#include "clientversion.h"

#include <boost/version.hpp>
#if BOOST_VERSION >= 105500 // Boost 1.55 or newer
#include <boost/predef.h>
#endif

BOOST_AUTO_TEST_SUITE(clientversion_tests)

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
    bool hideplatform = true;
    BOOST_CHECK(!OsInStr(XTSubVersion(0, "", {}, hideplatform)));

    hideplatform = false;
    std::string customUserAgent = "/test:1.0/";
    BOOST_CHECK(!OsInStr(XTSubVersion(0, customUserAgent, {}, hideplatform)));

#if BOOST_VERSION >= 105500
    hideplatform = false;
    BOOST_CHECK(OsInStr(XTSubVersion(0, "", {}, hideplatform)));
#endif
}

BOOST_AUTO_TEST_CASE(xtsubversion_customuseragent)
{
    BOOST_CHECK(XTSubVersion(0, "/test:1.0", {}, false).find("XT") == std::string::npos);
    BOOST_CHECK(XTSubVersion(0, "", {}, false).find("XT") != std::string::npos);
}

BOOST_AUTO_TEST_CASE(xtsubversion_uacomment)
{
    bool hideplatform = true;

    // only EB comment
    BOOST_CHECK(XTSubVersion(0, "", {}, hideplatform).find("(EB0)") != std::string::npos);

    // uacomments + EB
    std::vector<std::string> uacomments{"hello", "world"};
    BOOST_CHECK(XTSubVersion(0, "", uacomments, hideplatform).find("(hello; world; EB0)") != std::string::npos);

#if BOOST_VERSION >= 105500
    // combines with platform
    hideplatform = false;
    std::string withplatform = XTSubVersion(0, "", uacomments, hideplatform);
    BOOST_CHECK(withplatform.find("(hello; world; ") != std::string::npos);
    BOOST_CHECK(OsInStr(withplatform.substr(withplatform.find("(hello; world; "))));
#endif

    // not supported with custom user agent
    auto customUserAgent = "/test:1.0/";
    BOOST_CHECK(XTSubVersion(0, customUserAgent, uacomments, hideplatform).find("(hello; world") == std::string::npos);
}

BOOST_AUTO_TEST_CASE(xtsubversion_eb)
{
    // 1MB blocks
    BOOST_CHECK(XTSubVersion(1000000, "", {}, false).find("EB1)") != std::string::npos);
    // 1GB blocks
    BOOST_CHECK(XTSubVersion(1000 * 1000000, "", {}, false).find("EB1000)") != std::string::npos);
}

BOOST_AUTO_TEST_CASE(xtsubversion_customagent) {
    std::string useragent = "/hello world:0.1/";
    BOOST_CHECK_EQUAL(useragent, XTSubVersion(1000000, useragent, {}, false));
}

BOOST_AUTO_TEST_SUITE_END()
