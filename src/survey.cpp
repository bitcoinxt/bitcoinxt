#include "survey.h"
#include "clientversion.h"
#include "chainparams.h"
#include "curl_wrapper.h"
#include "scheduler.h"
#include "ui_interface.h"
#include "util.h"
#include "utilstrencodings.h"
#include <boost/algorithm/string.hpp>

static const std::string SURVEY_URL = "https://survey.bitcoinxt.software/v1/hello";
static const int DEFAULT_SURVEY_POLL_INTERVAL = 60 * 60 * 24;  // Once per day

void VersionNotifier::Notify(const std::string& newver, bool isImportant)
{

    std::string msg = _("Info: A new version of Bitcoin XT is available.");
    std::string important = _("This version is an important upgrade! Please upgrade now!");
    std::string verstr = _("New version is: ");
    verstr += newver;

    Log("*** " + msg + " " + (isImportant ? important + " " : " ") + verstr + "\n");

    static bool popupShown = false;
 
    if (!isImportant && !popupShown)
        ShowUIInfo(msg + " " + verstr);

    if (isImportant) {
        // Ignore popupShown. Bug the user every day.
        ShowUIWarning(msg + " " + important + " " + verstr);

        // We only replace misc warning if it's important.
        // We don't want to replace what might be an important warning needlessly.
        SetMiscWarning(msg + " " + important);
    }
    popupShown = true;
}

void VersionNotifier::ShowUIWarning(const std::string& msg) {
    uiInterface.ThreadSafeMessageBox(msg, "", CClientUIInterface::MSG_WARNING);
}

void VersionNotifier::ShowUIInfo(const std::string& msg) {
    uiInterface.ThreadSafeMessageBox(msg, "", CClientUIInterface::MSG_INFORMATION);
}

void VersionNotifier::SetMiscWarning(const std::string& w) { strMiscWarning = w; }
void VersionNotifier::Log(const std::string& s) { LogPrintf(s.c_str()); }

Survey::Survey(CScheduler& scheduler, 
        std::auto_ptr<VersionNotifier> v, 
        std::auto_ptr<CurlWrapper> c) :
    verNotifier(v), curl(c) {

    scheduler.scheduleFromNow(boost::bind(&Survey::pollSurvey, this), 0);
    scheduler.scheduleEvery(boost::bind(&Survey::pollSurvey, this),
            GetArg("-poll-survey-seconds", DEFAULT_SURVEY_POLL_INTERVAL));
}

Survey::~Survey() { }

void Survey::pollSurvey() {

    bool listening = GetBoolArg("-listen", false);
    int port = GetArg("-port", Params().GetDefaultPort());
    std::stringstream url;
    url << SURVEY_URL << "?v=" << CLIENT_VERSION << "&listening=" 
        << (listening ? "true" : "false");
    if (listening)
        url << "&port=" << port;

    std::pair<int, std::string> res = curl->fetchURL(url.str());
    if (res.first != 200) {
        LogPrintf("Failed to poll '%s': %d\n", url.str().c_str(), res.first);
        return;
    }

    bool important = false;
    bool newAvailable = false;
    std::string newVersion;
    
    std::vector<std::string> vars;
    boost::split(vars, res.second, boost::is_any_of(";"));
    if (vars.size() < 3) 
    {
        LogPrintf("Failed to parse survey response\n");
        return;
    }
    newAvailable = bool(atoi(vars[0]));
    important = bool(atoi(vars[1]));
    newVersion = vars[2];

    if (newAvailable)
        verNotifier->Notify(newVersion, important);
}
