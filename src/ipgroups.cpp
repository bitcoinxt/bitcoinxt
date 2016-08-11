// Copyright (c) 2009-2015 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <boost/foreach.hpp>
#include <boost/filesystem/fstream.hpp>
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
#include "net.h"

#include "torips.h"
#include "utilstrencodings.h"
#include "util.h"
#include "curl_wrapper.h"

using namespace std;

// shared_ptr as a workaround for last IPGroupSlot being
// destructed after cs_groups at program exit. IPGroupSlots keep a week ptr.
static boost::shared_ptr<CCriticalSection> cs_groups;
struct CSGroupsInit {
    CSGroupsInit() { cs_groups.reset(new CCriticalSection); }
} csgroupinit_instance;

static vector<CIPGroup> groups;

static const int OPEN_PROXY_PRIORITY = -10;


static const int DEFAULT_IP_PRIO_URL_POLL_INTERVAL = 60 * 60 * 24;  // Once per day is plenty.
static const int DEFAULT_IP_PRIO_SRC_POLL_INTERVAL = 60;            // Once per minute for local files is fine.

extern bool fListen;

// Returns the empty/default group if the IP does not belong to any group.
CIPGroupData FindGroupForIP(CNetAddr ip) {
    LOCK(*cs_groups);
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

vector<CSubNet> ParseIPData(string input) {
    vector<string> lines;
    vector<CSubNet> results;
    boost::split(lines, input, boost::is_any_of("\n"));
    for (size_t i = 0; i < lines.size(); i++) {
        // Skip empty lines and comments.
        if (lines[i].size() == 0) continue;
        if (lines[i][0] == '#') continue;

        CSubNet subNet(lines[i]);
        if (!subNet.IsValid()) {
            LogPrintf("Failed to parse IP address from IP data file at line %d: %s\n", i, SanitizeString(lines[i]));
            continue;
        }
        results.push_back(subNet);
    }
    return results;
}

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
        X509_STORE_add_cert(store, cert);
    } else {
        LogPrintf("NULL cert store");
    }
 
    X509_free(cert);
    BIO_free(bio);
    return CURLE_OK;
}

static CIPGroup *LoadIPDataFromWeb(const string &url, const string &groupname, int priority) {
    try {
        std::unique_ptr<CurlWrapper> curlWrapper = MakeCurl();
        curl_easy_setopt(curlWrapper->getHandle(), CURLOPT_SSL_CTX_FUNCTION, &ssl_context_setup);

        // This will block until the download is done.
        std::string res = curlWrapper->fetchURL(url);
        LogPrintf("IP list download succeeded from %s\n", url.c_str());

        vector<CSubNet> subnets = ParseIPData(res);
        if (subnets.size() > 0) {
            CIPGroup *ipGroup = new CIPGroup;
            ipGroup->header.name = groupname;
            ipGroup->header.priority = priority;
            ipGroup->subnets.assign(subnets.begin(), subnets.end());
            return ipGroup;
        }
    }
    catch (const curl_error& e) {
        LogPrintf("Failed to download IP priority data from %s: %s\n",
                e.url.c_str(), e.what());
    }
    return NULL;
}

static CIPGroup *LoadTorIPsFromWeb() {
    string ourip = "255.255.255.255";
    {
        LOCK(cs_mapLocalHost);
        // Just use the first IPv4 address for now. We could try all of them later on.
        BOOST_FOREACH(const PAIRTYPE(CNetAddr, LocalServiceInfo) &item, mapLocalHost)
        {
            LogPrintf("Local IP: %s\n", item.first.ToString());
            if (item.first.IsIPv4()) {
                ourip = item.first.ToStringIP();
                break;
            }
        }
    }
    string url = strprintf("https://check.torproject.org/torbulkexitlist?ip=%s&port=8333", ourip);
    return LoadIPDataFromWeb(url, "tor", OPEN_PROXY_PRIORITY);
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
    LOCK(*cs_groups);
    groups.push_back(ipGroup);
}

void AddOrReplace(CIPGroup &group) {
    LOCK(*cs_groups);
    // Try to replace existing group with same name.
    for (size_t i = 0; i < groups.size(); i++) {
        if (groups[i].header.name == group.header.name) {
            int connCount = groups[i].header.connCount;
            groups[i] = group;
            groups[i].header.connCount = connCount;
            return;
        }
    }
    groups.insert(groups.begin(), group);
}

static void MaybeRemoveGroup(const string &group_name) {
    LOCK(*cs_groups);
    for (size_t i = 0; i < groups.size(); i++) {
        if (groups[i].header.name == group_name) {
            groups.erase(groups.begin() + i);
            return;
        }
    }
}

static bool LoadIPGroupsFromFile(const boost::filesystem::path &path, const string &group_name, int priority) {
    boost::filesystem::ifstream stream(path);
    if (!stream.good()) {
        LogPrintf("IP Priority: Unable to read specified IP priority source file %s\n", path.string());
        return false;
    }

    stringstream buf;
    buf << stream.rdbuf();

    CIPGroup group;
    group.header.name = group_name;
    group.header.priority = priority;

    group.subnets = ParseIPData(buf.str());
    if (group.subnets.size() == 0) {
        LogPrintf("IP Priority: File empty or unable to understand the contents of %s\n", path.string());
        return false;
    }

    AddOrReplace(group);
    return true;
}

void InitIPGroupsFromCommandLine() {
    // Allow IP groups to be loaded from a file passed in via the command line.
    vector<string> &srcs = mapMultiArgs[IP_PRIO_SRC_FLAG_NAME];
    BOOST_FOREACH(const string &src, srcs)
    {
        vector<string> pieces;
        boost::split(pieces, src, boost::is_any_of(","));
        if (pieces.size() != 3) {
            LogPrintf("The -ip-priority-source flag takes a triple of name,priority,file_name: you gave: %s\n", src);
            continue;
        }

        string &group_name = pieces[0];
        int prio = atoi(pieces[1].c_str());
        if (prio == 0) {
            LogPrintf("IP Priority: Could not parse %s as an integer or priority of zero would be useless\n", pieces[1]);
            continue;
        }
        string &file_name = pieces[2];

        boost::filesystem::path file_path(file_name);
        boost::filesystem::path path = file_path.is_complete() ? file_path : GetDataDir(false) / file_name;

        if (!LoadIPGroupsFromFile(path, group_name, prio)) {
            MaybeRemoveGroup(group_name);
        }
    }
}

static void PollTorWebsite() {
    boost::scoped_ptr<CIPGroup> group(LoadTorIPsFromWeb());
    if (group)
        AddOrReplace(*group.get());
}

static void InitTorIPGroups(CScheduler *scheduler) {
    // Load Tor Exits as a group. We use both static and dynamically loaded data to avoid incentivising someone
    // to DoS the Tor website, and to give us a fighting chance of correctly labelling connections that come in
    // a few seconds after startup (this can happen). Kick off a poll now and then do it (by default) once per day.
    if (scheduler) {
        scheduler->scheduleFromNow(&PollTorWebsite, 0);
        scheduler->scheduleEvery(&PollTorWebsite, GetArg("-poll-tor-seconds", DEFAULT_IP_PRIO_URL_POLL_INTERVAL));
    }
    LoadTorIPsFromStaticData();
}

void InitIPGroups(CScheduler *scheduler) {
    {
        LOCK(*cs_groups);
        groups.clear();
    }

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
    InitIPGroupsFromCommandLine();
    if (scheduler)
        scheduler->scheduleEvery(&InitIPGroupsFromCommandLine, GetArg("-poll-ip-sources-seconds", DEFAULT_IP_PRIO_SRC_POLL_INTERVAL));

    // Future ideas:
    // - Load IP ranges from URLs to let node admin deprioritise ranges that seem to be jamming.
    // - Dialback on connect to see if an IP is listening/responding to port 8333: if so, more likely
    //   to be a valuable peer than an attacker, so bump priority.
    // - Raise priority of a connection as it does valuable things like relaying data to us.
    // - Design a protocol to let user wallets gain priority by proving ownership of coin age, as they tend to have
    //   short lived connections and roam around different IPs, but we still want to serve them ahead of long term
    //   idling connections.
}

IPGroupSlot::IPGroupSlot(const std::string& groupName) :
    groupName(groupName), groupCS(cs_groups)
{
    LOCK(*cs_groups);

    typedef std::vector<CIPGroup>::iterator auto_;
    for (auto_ g = groups.begin(); g != groups.end(); ++g) {
        if (g->header.name != groupName)
            continue;

        g->header.connCount += 1;

        if (g->header.decrPriority)
            g->header.priority -= IPGROUP_CONN_MODIFIER;

        return; // names are unique, so we're done.
    }
    assert(!"tried to create a groupslot for a group that does not exist");
}

IPGroupSlot::~IPGroupSlot() {
    // groupCS can be null when exiting bitcoind due to CNetCleanup
    // instance in net.cpp destructing after cs_groups.
    boost::shared_ptr<CCriticalSection> cs(groupCS.lock());
    if (cs.get() == NULL)
        return;

    LOCK(*cs);

    typedef std::vector<CIPGroup>::iterator auto_;
    for (auto_ g = groups.begin(); g != groups.end(); ++g) {
        CIPGroupData& group = g->header;

        if (group.name != groupName)
            continue;

        group.connCount -= 1;
        assert(group.connCount >= 0);

        if (group.decrPriority)
            group.priority += IPGROUP_CONN_MODIFIER;

        if (group.selfErases && !group.connCount)
            MaybeRemoveGroup(g->header.name);

        return;
    }
}

CIPGroupData IPGroupSlot::Group() {
    LOCK(*cs_groups);
    typedef std::vector<CIPGroup>::iterator auto_;
    for (auto_ g = groups.begin(); g != groups.end(); ++g)
        if (g->header.name == groupName)
            return g->header;

    return CIPGroupData();
}

std::unique_ptr<IPGroupSlot> AssignIPGroupSlot(const CNetAddr& ip) {
    LOCK(*cs_groups);

    CIPGroupData group = FindGroupForIP(ip);

    // IP belongs to a group already.
    if (!group.name.empty())
        return std::unique_ptr<IPGroupSlot>(new IPGroupSlot(group.name));

    // If IP does not belong to a group, create a new group
    // for only this IP. This group is used to de-prioritize multiple
    // connections from same IP.
    //
    // The goal is to force an attacker to spread out in order to get lots of priority.

    // IPGROUP_CONN_MODIFIER will be decremented again when slot is constructed.
    int pri = DEFAULT_IPGROUP_PRI + IPGROUP_CONN_MODIFIER;

    CIPGroup newGroup;
    newGroup.header = CIPGroupData(ip.ToStringIP(), pri, true, true);
    newGroup.subnets.push_back(CSubNet(ip.ToStringIP() + "/32"));
    groups.push_back(newGroup);

    return std::unique_ptr<IPGroupSlot>(new IPGroupSlot(newGroup.header.name));
}
