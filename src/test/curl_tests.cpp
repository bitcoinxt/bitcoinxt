// Copyright (c) 2015- The Bitcoin XT developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <boost/test/unit_test_suite.hpp>
#include <boost/test/test_tools.hpp>

#include <curl_wrapper.h>

BOOST_AUTO_TEST_SUITE(curl_tests);

BOOST_AUTO_TEST_CASE(dummy_curl)
{
    {
        std::unique_ptr<CurlWrapper> curl(new DummyCurlWrapper(200, "some data"));
        BOOST_CHECK_EQUAL(curl->fetchURL("http://example.org"), "some data");
    }
    {
        DummyCurlWrapper* dcurl = new DummyCurlWrapper(200, "more data");
        std::unique_ptr<CurlWrapper> curl(dcurl);
        curl->fetchURL("http://example.org/2");
        BOOST_CHECK_EQUAL(dcurl->lastUrl, "http://example.org/2");
        curl->fetchURL("http://example.org/3");
        BOOST_CHECK_EQUAL(dcurl->lastUrl, "http://example.org/3");
    }
}

BOOST_AUTO_TEST_CASE(dummy_curl_throws) {
    // Throw if not HTTP status 200 OK
    std::unique_ptr<CurlWrapper> curl(new DummyCurlWrapper(404, "some data"));
    BOOST_CHECK_THROW(curl->fetchURL("http://example.lrg"), curl_error);
}

BOOST_AUTO_TEST_SUITE_END()

