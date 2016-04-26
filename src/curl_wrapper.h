#ifndef BITCOIN_CURLWRAPPER_H
#define BITCOIN_CURLWRAPPER_H

#include <string>
#include <memory>
#include <stdexcept>

typedef void CURL;

struct curl_error : public std::runtime_error {
    curl_error(const std::string& url, const std::string& err) : runtime_error(err), url(url) { }
    virtual ~curl_error() throw() { }
    std::string url;
};

struct CurlWrapper {

    // Returns contents of url.
    // Throws curl_error on curl error, or status code other than "200 OK".
    virtual std::string fetchURL(const std::string& url) = 0;
    virtual CURL* getHandle() = 0;
    virtual ~CurlWrapper() = 0;
};
inline CurlWrapper::~CurlWrapper() { }

// Dummy for unittesting.
struct DummyCurlWrapper : public CurlWrapper {

    DummyCurlWrapper(int statuscode, const std::string& content);

    virtual std::string fetchURL(const std::string& url);
    virtual CURL* getHandle() { return NULL; }

    int code;
    std::string content;
    std::string lastUrl; // last url used with fetchURL
};

std::unique_ptr<CurlWrapper> MakeCurl();

#endif
