#ifndef BITCOIN_OPTIONS_H
#define BITCOIN_OPTIONS_H

#include <memory>
#include <stdint.h>
#include <string>
#include <vector>

struct Opt {
    Opt();

    bool IsStealthMode();
    bool HidePlatform();
    std::vector<std::string> UAComment(bool validate = false);
    int ScriptCheckThreads();
};

/** Maximum number of script-checking threads allowed */
static const int MAX_SCRIPTCHECK_THREADS = 16;
/** -par default (number of script-checking threads, 0 = auto) */
static const int DEFAULT_SCRIPTCHECK_THREADS = 0;


//
// For unit testing
//

struct ArgGetter {
    virtual bool GetBool(const std::string& arg, bool def) = 0;
    virtual std::vector<std::string> GetMultiArgs(const std::string& arg) = 0;
    virtual int64_t GetArg(const std::string& strArg, int64_t nDefault) = 0;
};

struct ArgReset {
    ~ArgReset();
};

// Temporary replace the global getter for fetching user configurations.
//
// Returns a RAII object that sets system back to default state.
std::auto_ptr<ArgReset> SetDummyArgGetter(std::auto_ptr<ArgGetter>);

#endif
