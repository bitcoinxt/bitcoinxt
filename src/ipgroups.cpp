// Copyright (c) 2009-2015 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <boost/foreach.hpp>
#include <boost/algorithm/string.hpp>
#include <curl/curl.h>
#include "ipgroups.h"
#include "sync.h"
#include "scheduler.h"

#include "torips.h"
#include "utilstrencodings.h"
#include "util.h"

using namespace std;

CCriticalSection cs_groups;

static vector<CIPGroup> groups;

// As of July 2015 the Tor IPs document is only 169kb in size so this is plenty.
static const size_t MAX_DOWNLOAD_SIZE = 1024 * 1024;
static const int OPEN_PROXY_PRIORITY = -10;
static const int DEFAULT_TOR_POLL_INTERVAL = 60 * 60 * 24;  // Once per day is plenty.

// Returns NULL if the IP does not belong to any group.
CIPGroupData FindGroupForIP(CNetAddr ip) {
    LOCK(cs_groups);
    if (!ip.IsValid()) {
        LogPrintf("IP is not valid: %s\n", ip.ToString());
        return CIPGroupData();  // Default.
    }

    BOOST_FOREACH(CIPGroup &group, groups)
    {
        BOOST_FOREACH(const CSubNet &subNet, group.subnets)
        {
            if (subNet.Match(ip))
                return group.header;
        }
    }
    return CIPGroupData();
}

static size_t CurlData(void *ptr, size_t size, size_t nmemb, void *user_ptr) {
    string *data = static_cast<string*>(user_ptr);
    string buf(static_cast<const char *>(ptr), size*nmemb);
    size_t realsize = size * nmemb;
    if (data->size() + realsize > MAX_DOWNLOAD_SIZE)
        return 0;  // Abort.
    data->append(buf);
    return realsize;
}

vector<CSubNet> ParseTorData(string input) {
    vector<string> words;
    vector<CSubNet> results;
    boost::split(words, input, boost::is_any_of("\n "));
    for (size_t i = 0; i < words.size(); i++) {
        if (words[i] == "ExitAddress") {
            if (++i == words.size()) {
                LogPrintf("Failed to parse Tor data: truncated\n");
                results.clear();
                return results;
            }
            CSubNet subNet(words[i]);
            if (!subNet.IsValid() || words[i].find("/") != string::npos) {
                LogPrintf("Failed to parse IP address from Tor data at word %d: %s\n", i, SanitizeString(words[i]));
                results.clear();
                return results;
            }
            results.push_back(subNet);
        }
    }
    return results;
}

struct CurlWrapper {
    CURL *handle;

    CurlWrapper() {
        CURLcode rv = curl_global_init(CURL_GLOBAL_ALL);
        if (rv != CURLE_OK) {
            LogPrintf("libcurl failed to initialize, cannot load Tor IPs: %s\n", curl_easy_strerror(rv));
            handle = NULL;
        } else {
            handle = curl_easy_init();
        }
    }

    ~CurlWrapper() {
        if (handle != NULL) {
            curl_easy_cleanup(handle);
            curl_global_cleanup();
        }
    }
};

static CIPGroup *LoadTorIPsFromWeb() {
    std::string data;

    CurlWrapper curlWrapper;
    CURL *curl = curlWrapper.handle;
    if (curl == NULL)
        return NULL;

    // If -curl-verbose is specified, debug info and headers will be printed to stderr.
    curl_easy_setopt(curl, CURLOPT_VERBOSE, GetBoolArg("-curl-verbose", false) ? 1L : 0L);
    curl_easy_setopt(curl, CURLOPT_HEADER, 0L);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);   // Follow redirects
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Bitcoin XT");
    curl_easy_setopt(curl, CURLOPT_FORBID_REUSE, 1L);   // Don't use up sockets on check.torproject.org unnecessarily.
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &CurlData);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &data);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_URL, "https://check.torproject.org/exit-addresses");

    // This will block until the download is done.
    CURLcode rv = curl_easy_perform(curl);
    if (rv == CURLE_OK) {
        LogPrintf("Tor IPs download succeeded\n");
        std::vector<CSubNet> subnets = ParseTorData(data);
        if (subnets.size() > 0) {
            CIPGroup *ipGroup = new CIPGroup;
            ipGroup->header.name = "tor";
            ipGroup->header.priority = OPEN_PROXY_PRIORITY;
            ipGroup->subnets.assign(subnets.begin(), subnets.end());
            return ipGroup;
        }
    } else {
        LogPrintf("Failed to download Tor exit IPs: %s\n", curl_easy_strerror(rv));
    }
    return NULL;
}

static void LoadTorIPsFromStaticData() {
    CIPGroup ipGroup;
    ipGroup.header.name = "tor_static";
    ipGroup.header.priority = OPEN_PROXY_PRIORITY;
    for (const char **ptr = pszTorExits; *ptr; ptr++) {
        string ip(*ptr);
        ip = ip + "/32";
        ipGroup.subnets.push_back(CSubNet(ip));
    }
    LOCK(cs_groups);
    groups.push_back(ipGroup);
}

static void PollTorWebsite() {
    boost::scoped_ptr<CIPGroup> group(LoadTorIPsFromWeb());
    if (group) {
        LOCK(cs_groups);
        // Try to replace existing.
        for (size_t i = 0; i < groups.size(); i++) {
            if (groups[i].header.name == "tor") {
                groups[i] = *group.get();
                return;
            }
        }
        groups.push_back(*group.get());
    }
}

void InitIPGroups(CScheduler *scheduler) {
    // Load Tor Exits as a group. We use both static and dynamically loaded data to avoid incentivising someone
    // to DoS the Tor website, and to give us a fighting chance of correctly labelling connections that come in
    // a few seconds after startup (this can happen). Kick off a poll now and then do it (by default) once per day.
    if (scheduler) {
        scheduler->scheduleFromNow(&PollTorWebsite, 0);
        scheduler->scheduleEvery(&PollTorWebsite, GetArg("-poll-tor-seconds", DEFAULT_TOR_POLL_INTERVAL));
    }
    LoadTorIPsFromStaticData();
}
