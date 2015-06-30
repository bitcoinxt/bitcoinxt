// Copyright (c) 2009-2015 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <boost/foreach.hpp>
#include <boost/filesystem.hpp>
#include <boost/algorithm/string.hpp>
#ifdef WIN32
#define CURL_STATICLIB
#endif
#include <curl/curl.h>
#include <openssl/ssl.h>

#include "ipgroups.h"
#include "sync.h"
#include "scheduler.h"
#include "chainparams.h"

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

extern bool fListen;

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

// Hack around the fact that some platforms, especially on Linux, have root stores
// that openssl/curl can't find by default. This is the root cert used by check.torproject.org
// as of July 2015. We add it to the cert store along with the system certs.
static const char *digicert_root_ca = 
    "-----BEGIN CERTIFICATE-----\n" \
    "MIIDxTCCAq2gAwIBAgIQAqxcJmoLQJuPC3nyrkYldzANBgkqhkiG9w0BAQUFADBs\n" \
    "MQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3\n" \
    "d3cuZGlnaWNlcnQuY29tMSswKQYDVQQDEyJEaWdpQ2VydCBIaWdoIEFzc3VyYW5j\n" \
    "ZSBFViBSb290IENBMB4XDTA2MTExMDAwMDAwMFoXDTMxMTExMDAwMDAwMFowbDEL\n" \
    "MAkGA1UEBhMCVVMxFTATBgNVBAoTDERpZ2lDZXJ0IEluYzEZMBcGA1UECxMQd3d3\n" \
    "LmRpZ2ljZXJ0LmNvbTErMCkGA1UEAxMiRGlnaUNlcnQgSGlnaCBBc3N1cmFuY2Ug\n" \
    "RVYgUm9vdCBDQTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBAMbM5XPm\n" \
    "+9S75S0tMqbf5YE/yc0lSbZxKsPVlDRnogocsF9ppkCxxLeyj9CYpKlBWTrT3JTW\n" \
    "PNt0OKRKzE0lgvdKpVMSOO7zSW1xkX5jtqumX8OkhPhPYlG++MXs2ziS4wblCJEM\n" \
    "xChBVfvLWokVfnHoNb9Ncgk9vjo4UFt3MRuNs8ckRZqnrG0AFFoEt7oT61EKmEFB\n" \
    "Ik5lYYeBQVCmeVyJ3hlKV9Uu5l0cUyx+mM0aBhakaHPQNAQTXKFx01p8VdteZOE3\n" \
    "hzBWBOURtCmAEvF5OYiiAhF8J2a3iLd48soKqDirCmTCv2ZdlYTBoSUeh10aUAsg\n" \
    "EsxBu24LUTi4S8sCAwEAAaNjMGEwDgYDVR0PAQH/BAQDAgGGMA8GA1UdEwEB/wQF\n" \
    "MAMBAf8wHQYDVR0OBBYEFLE+w2kD+L9HAdSYJhoIAu9jZCvDMB8GA1UdIwQYMBaA\n" \
    "FLE+w2kD+L9HAdSYJhoIAu9jZCvDMA0GCSqGSIb3DQEBBQUAA4IBAQAcGgaX3Nec\n" \
    "nzyIZgYIVyHbIUf4KmeqvxgydkAQV8GK83rZEWWONfqe/EW1ntlMMUu4kehDLI6z\n" \
    "eM7b41N5cdblIZQB2lWHmiRk9opmzN6cN82oNLFpmyPInngiK3BD41VHMWEZ71jF\n" \
    "hS9OMPagMRYjyOfiZRYzy78aG6A9+MpeizGLYAiJLQwGXFK3xPkKmNEVX58Svnw2\n" \
    "Yzi9RKR/5CYrCsSXaQ3pjOLAEFe4yHYSkVXySGnYvCoCWw9E1CAx2/S6cCZdkGCe\n" \
    "vEsXCS+0yx5DaMkHJ8HSXPfqIbloEpw8nL+e/IBcm2PN7EeqJSdnoDfzAIJ9VNep\n" \
    "+OkuE6N36B9K\n" \
    "-----END CERTIFICATE-----";

static CURLcode ssl_context_setup(CURL *curl, void *sslctx, void *param) {
    BIO *bio = BIO_new_mem_buf((void *)digicert_root_ca, -1);
    X509 *cert = NULL;
    PEM_read_bio_X509(bio, &cert, 0, NULL);
    if (cert == NULL) {
        // This should never happen.
        LogPrintf("ipgroups: Could not parse hard-coded cert\n");
        BIO_free(bio);
        return CURLE_OK;   // Proceed anyway.
    }

    assert(sslctx);

    // Get a pointer to the X509 certificate store (which may be empty!)
    X509_STORE *store = SSL_CTX_get_cert_store((SSL_CTX *)sslctx);
    if (store != NULL) {
        // And add. We don't check the return code because we don't care: this is
        // strictly best effort only.
        int ret = X509_STORE_add_cert(store, cert);
        LogPrintf("Tor IPs: adding cert to store: %d\n", ret);
    } else {
        LogPrintf("NULL cert store");
    }
 
    X509_free(cert);
    BIO_free(bio);
    return CURLE_OK;
}

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
    curl_easy_setopt(curl, CURLOPT_SSL_CTX_FUNCTION, &ssl_context_setup);
    curl_easy_setopt(curl, CURLOPT_URL, "https://check.torproject.org/exit-addresses");

    // curl/openssl seem to know how to use the platform CA store on win/mac.
#if !defined(WIN32) && !defined(MAC_OSX)
    // On Linux/BSD/etc try a couple of places we know CA bundles are often kept.
    if (boost::filesystem::exists(boost::filesystem::path("/etc/ssl/certs/ca-certificates.crt"))) {
        curl_easy_setopt(curl, CURLOPT_CAINFO, "/etc/ssl/certs/ca-certificates.crt");
    } else if (boost::filesystem::exists(boost::filesystem::path("/etc/ssl/certs/ca-bundle.crt"))) {
        curl_easy_setopt(curl, CURLOPT_CAINFO, "/etc/ssl/certs/ca-bundle.crt");
    } else {
        // Disable use of the system CA store :( We can't simply present our hard coded cert to be used in addition
        // because the curl API just doesn't support that. It's one or the other. Our callback function will configure
        // the cert used by the website as of the time of writing.
        curl_easy_setopt(curl, CURLOPT_CAPATH, NULL);
        curl_easy_setopt(curl, CURLOPT_CAINFO, NULL);
    }
#endif

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

static void InitTorIPGroups(CScheduler *scheduler) {
    // Load Tor Exits as a group. We use both static and dynamically loaded data to avoid incentivising someone
    // to DoS the Tor website, and to give us a fighting chance of correctly labelling connections that come in
    // a few seconds after startup (this can happen). Kick off a poll now and then do it (by default) once per day.
    if (scheduler) {
        scheduler->scheduleFromNow(&PollTorWebsite, 0);
        scheduler->scheduleEvery(&PollTorWebsite, GetArg("-poll-tor-seconds", DEFAULT_TOR_POLL_INTERVAL));
    }
    LoadTorIPsFromStaticData();
}

void InitIPGroups(CScheduler *scheduler) {
    // If scheduler is NULL then we're in unit tests.
    if (scheduler) {
        // Don't use in regtest mode to avoid excessive and useless HTTP requests, don't use if the user disabled.
        if (GetBoolArg("-disableipprio", false) || Params().NetworkIDString() == "regtest" || !fListen)
            return;
        // If we have a proxy, then we are most likely not reachable from the internet, so don't use.
        proxyType dummy;
        if (GetProxy(NET_IPV4, dummy) || GetProxy(NET_IPV6, dummy) || GetProxy(NET_TOR, dummy))
            return;
    }

    InitTorIPGroups(scheduler);

    // Future ideas:
    // - Load IP ranges from files to let node admin deprioritise ranges that seem to be jamming.
    // - Dialback on connect to see if an IP is listening/responding to port 8333: if so, more likely
    //   to be a valuable peer than an attacker, so bump priority.
    // - Raise priority of a connection as it does valuable things like relaying data to us.
    // - Design a protocol to let user wallets gain priority by proving ownership of coin age, as they tend to have
    //   short lived connections and roam around different IPs, but we still want to serve them ahead of long term
    //   idling connections.
}
