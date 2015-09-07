// Copyright (c) 2015- The Bitcoin XT developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/test/unit_test_suite.hpp>
#include <boost/test/test_tools.hpp>
#include <boost/algorithm/string/predicate.hpp>

#include "curl_wrapper.h"
#include "clientversion.h"
#include "ipgroups.h"
#include "scheduler.h"
#include "survey.h"
#include "util.h"

#include <memory>
//#include "test/test_bitcoin.h"


extern std::vector<CSubNet> ParseIPData(std::string input);

BOOST_AUTO_TEST_SUITE(survey_tests);

namespace {
    std::string surveyResp(bool newVerison, bool isImportant) {
        // new version available;is important;latest version string
        std::stringstream ss;
        ss << (newVerison ? 1 : 0) << ";"
           << (isImportant ? 1 : 0) << ";"
           << "latest and greatest!";
        return ss.str();
    }

    struct DummyScheduler : public CScheduler {
        DummyScheduler() : scheduledEvery(-1) { }

        void scheduleFromNow(Function f, int64_t) {
            f();
        }
        void scheduleEvery(Function f, int64_t deltaSeconds) {
            scheduledEvery = deltaSeconds;
        }

        int64_t scheduledEvery;
    };

    struct DummyVersionNotifier : public VersionNotifier {
        virtual void ShowUIWarning(const std::string&) { uiwarn++; }
        virtual void ShowUIInfo(const std::string&) { uiinfo++; }
        virtual void SetMiscWarning(const std::string&) { miscwarn++; }
        virtual void Log(const std::string&) { log++; }

        int uiwarn;
        int uiinfo;
        int miscwarn;
        int log;
    };

    std::auto_ptr<VersionNotifier> MakeDummyVer() {
        return std::auto_ptr<VersionNotifier>(new DummyVersionNotifier());
    }

} // ns anon

BOOST_AUTO_TEST_CASE(survey_no_new_version)
{
    DummyScheduler s;
    DummyVersionNotifier *v = new DummyVersionNotifier();
    Survey survey(s, std::auto_ptr<VersionNotifier>(v),
            MakeDummyCurl(200, surveyResp(false, false)));

    // Should not have notified the user of anything.
    BOOST_CHECK_EQUAL(v->uiwarn, 0);
    BOOST_CHECK_EQUAL(v->uiinfo, 0);
    BOOST_CHECK_EQUAL(v->miscwarn, 0);
    BOOST_CHECK_EQUAL(v->log, 0);
}

BOOST_AUTO_TEST_CASE(survey_new_version)
{
    DummyScheduler s;
    DummyVersionNotifier *v = new DummyVersionNotifier();
    Survey survey(s, std::auto_ptr<VersionNotifier>(v),
            MakeDummyCurl(200, surveyResp(true, false)));

    // Should have notified the user, but not warned.
    BOOST_CHECK_EQUAL(v->uiwarn, 0);
    BOOST_CHECK_EQUAL(v->uiinfo, 1);
    BOOST_CHECK_EQUAL(v->miscwarn, 0);
    BOOST_CHECK_EQUAL(v->log, 1);
}

BOOST_AUTO_TEST_CASE(survey_new_important_version)
{
    DummyScheduler s;
    DummyVersionNotifier *v = new DummyVersionNotifier();
    Survey survey(s, std::auto_ptr<VersionNotifier>(v),
            MakeDummyCurl(200, surveyResp(true, true)));

    // Should have warned the user.
    BOOST_CHECK_EQUAL(v->uiwarn, 1);
    BOOST_CHECK_EQUAL(v->uiinfo, 0);
    BOOST_CHECK_EQUAL(v->miscwarn, 1);
    BOOST_CHECK_EQUAL(v->log, 1);
}

BOOST_AUTO_TEST_CASE(survey_important_not_new)
{
    DummyScheduler s;
    DummyVersionNotifier *v = new DummyVersionNotifier();
    Survey survey(s, std::auto_ptr<VersionNotifier>(v),
            MakeDummyCurl(200, surveyResp(false, true)));

    // Important but not new? Does not make sense. Don't notify.
    BOOST_CHECK_EQUAL(v->uiwarn, 0);
    BOOST_CHECK_EQUAL(v->uiinfo, 0);
    BOOST_CHECK_EQUAL(v->miscwarn, 0);
    BOOST_CHECK_EQUAL(v->log, 0);
}

BOOST_AUTO_TEST_CASE(survey_404s) {
    DummyScheduler s;
    DummyVersionNotifier *v = new DummyVersionNotifier();
    Survey survey(s, std::auto_ptr<VersionNotifier>(v),
            MakeDummyCurl(404, ""));

    BOOST_CHECK_EQUAL(v->uiwarn, 0);
    BOOST_CHECK_EQUAL(v->uiinfo, 0);
    BOOST_CHECK_EQUAL(v->miscwarn, 0);
    BOOST_CHECK_EQUAL(v->log, 0);

}

BOOST_AUTO_TEST_CASE(url_correct_params) {
    DummyScheduler s;
    {
        std::stringstream exp;
        exp << "?v=" << CLIENT_VERSION << "&listening=false";
        
        std::auto_ptr<CurlWrapper> c = MakeDummyCurl(200, "");
        CurlWrapper* cptr = c.get();

        const char *argv[] = {"ignored", "-listen=0"};
        ParseParameters(2, argv);

        Survey survey(s, MakeDummyVer(), c);
        BOOST_CHECK(boost::algorithm::ends_with(cptr->getLastURL(), exp.str()));
    }

    { 
        std::stringstream exp;
        exp << "?v=" << CLIENT_VERSION << "&listening=true&port=1234";
        
        const char *argv[] = {"ignored", "-listen=1", "-port=1234"};
        ParseParameters(3, argv);
        
        std::auto_ptr<CurlWrapper> c = MakeDummyCurl(200, "");
        CurlWrapper* cptr = c.get();

        Survey survey(s, MakeDummyVer(), c);

        BOOST_CHECK(boost::algorithm::ends_with(cptr->getLastURL(), exp.str()));
    }
}

BOOST_AUTO_TEST_CASE(garbage_does_nothing) {
    DummyScheduler s;
    DummyVersionNotifier *v = new DummyVersionNotifier();
    Survey survey(s, std::auto_ptr<VersionNotifier>(v),
            MakeDummyCurl(200, "garbage response"));

    BOOST_CHECK_EQUAL(v->uiwarn, 0);
    BOOST_CHECK_EQUAL(v->uiinfo, 0);
    BOOST_CHECK_EQUAL(v->miscwarn, 0);
    BOOST_CHECK_EQUAL(v->log, 0);
}

BOOST_AUTO_TEST_CASE(scheduled_once_per_day) {
    DummyScheduler s;
    Survey survey(s, MakeDummyVer(), MakeDummyCurl(200, ""));

    BOOST_CHECK_EQUAL(s.scheduledEvery, 60 * 60 * 24);
}

BOOST_AUTO_TEST_SUITE_END()
