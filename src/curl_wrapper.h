#ifndef BITCOIN_CURLWRAPPER_H
#define BITCOIN_CURLWRAPPER_H

#include <string>
#include <utility>
#include <memory>

typedef void CURL;

struct CurlWrapper {
    
    virtual std::pair<int, std::string> fetchURL(const std::string&) = 0;

    virtual CURL* getHandle() = 0;
    
    // for unit testing
    virtual std::string getLastURL() const = 0;
    
    virtual ~CurlWrapper();

};
inline CurlWrapper::~CurlWrapper() { }

std::auto_ptr<CurlWrapper> MakeCurl();
std::auto_ptr<CurlWrapper> MakeDummyCurl(int retcode, const std::string& resp);

#endif
