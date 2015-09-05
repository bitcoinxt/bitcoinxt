#include "curl_wrapper.h"
#include "util.h"
#ifdef WIN32
#define CURL_STATICLIB
#endif
#include <curl/curl.h>
#include <boost/filesystem.hpp>

using namespace std;

// As of July 2015 the Tor IPs document is only 169kb in size so this is plenty.
static const size_t MAX_DOWNLOAD_SIZE = 1024 * 1024;

struct CurlInitWrapper {
    CurlInitWrapper() {

        CURLcode rv = curl_global_init(CURL_GLOBAL_ALL);
        curlOK = (rv == CURLE_OK);

        if (!curlOK)
            LogPrintf("libcurl failed to initialize\n");
    }
    ~CurlInitWrapper() {
        curl_global_cleanup();
    }

    bool curlOK;
};

static size_t CurlData(void *ptr, size_t size, size_t nmemb, void *user_ptr) {
    string *data = static_cast<string*>(user_ptr);
    string buf(static_cast<const char *>(ptr), size*nmemb);
    size_t realsize = size * nmemb;
    if (data->size() + realsize > MAX_DOWNLOAD_SIZE)
        return 0;  // Abort.
    data->append(buf);
    return realsize;
}

std::string statusCodeErrorStr(int code) {
    std::stringstream ss;
    ss << "http status code was '" << code << "', wanted 200 OK";
    return ss.str();
}

struct CurlWrapperImpl : public CurlWrapper {
    CurlWrapperImpl(CURL* h) : handle(h) {
        assert(h);

        // If -curl-verbose is specified, debug info and headers will be printed to stderr.
        curl_easy_setopt(handle, CURLOPT_VERBOSE, GetBoolArg("-curl-verbose", false) ? 1L : 0L);
        curl_easy_setopt(handle, CURLOPT_HEADER, 0L);
        curl_easy_setopt(handle, CURLOPT_NOPROGRESS, 1L);
        curl_easy_setopt(handle, CURLOPT_NOSIGNAL, 1L);
        // Follow redirects
        curl_easy_setopt(handle, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(handle, CURLOPT_USERAGENT, "Bitcoin XT");
        // Don't use up sockets on the target website unnecessarily.
        curl_easy_setopt(handle, CURLOPT_FORBID_REUSE, 1L);

        // curl/openssl seem to know how to use the platform CA store on win/mac.
#if !defined(WIN32) && !defined(MAC_OSX)
        // On Linux/BSD/etc try a couple of places we know CA bundles are often kept.
        if (boost::filesystem::exists(boost::filesystem::path("/etc/ssl/certs/ca-certificates.crt"))) {
            curl_easy_setopt(handle, CURLOPT_CAINFO, "/etc/ssl/certs/ca-certificates.crt");
        } else if (boost::filesystem::exists(boost::filesystem::path("/etc/ssl/certs/ca-bundle.crt"))) {
            curl_easy_setopt(handle, CURLOPT_CAINFO, "/etc/ssl/certs/ca-bundle.crt");
        } else {
            // Disable use of the system CA store :( We can't simply
            // present our hard coded cert to be used in addition
            // because the curl API just doesn't support that. It's one or the
            // other. Our callback function will configure
            // the cert used by the website as of the time of writing.
            curl_easy_setopt(handle, CURLOPT_CAPATH, NULL);
            curl_easy_setopt(handle, CURLOPT_CAINFO, NULL);
        }
#endif
    }
    ~CurlWrapperImpl() {
        curl_easy_cleanup(handle);
    }

    virtual std::string fetchURL(const std::string& url) {
        std::string data;

        curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, &CurlData);
        curl_easy_setopt(handle, CURLOPT_WRITEDATA, &data);
        curl_easy_setopt(handle, CURLOPT_URL, url.c_str());

        CURLcode rv = curl_easy_perform(handle);
        if (rv != CURLE_OK)
            throw curl_error(url, curl_easy_strerror(rv));

        long httpCode;
        curl_easy_getinfo(handle, CURLINFO_RESPONSE_CODE, &httpCode);
        if (httpCode != 200)
            throw curl_error(url, statusCodeErrorStr(httpCode));

        return data;
    }

    virtual CURL* getHandle() { return handle; }

    private:
        CURL* handle;
};

struct BrokenCurl : public CurlWrapper {
    virtual std::string fetchURL(const std::string& url) {
        throw curl_error(url, "curl failed to initialize");
    }
    virtual CURL* getHandle() { return NULL; }
};

DummyCurlWrapper::DummyCurlWrapper(int statuscode, const std::string& content) :
    code(statuscode), content(content)
{
}

std::string DummyCurlWrapper::fetchURL(const std::string& url) {
    lastUrl = url;
    if (code != 200)
        throw curl_error(url, statusCodeErrorStr(code));

    return content;
}

std::auto_ptr<CurlWrapper> MakeCurl() {
    static CurlInitWrapper c;
    if (!c.curlOK)
        return std::auto_ptr<CurlWrapper>(new BrokenCurl);

    CURL* curl = curl_easy_init();
    return curl
        ? std::auto_ptr<CurlWrapper>(new CurlWrapperImpl(curl))
        : std::auto_ptr<CurlWrapper>(new BrokenCurl);
}
