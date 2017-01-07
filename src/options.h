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
    int64_t CheckpointDays();
    uint64_t MaxBlockSizeVote();

    // Thin block options
    bool UsingThinBlocks();
    bool AvoidFullBlocks();
    bool OptimalThinBlocksOnly();
    int ThinBlocksMaxParallel();
    bool PreferCompactBlocks() const; // Added for rpc test
};

/** Maximum number of script-checking threads allowed */
static const int MAX_SCRIPTCHECK_THREADS = 16;
/** -par default (number of script-checking threads, 0 = auto) */
static const int DEFAULT_SCRIPTCHECK_THREADS = 0;
// Blocks newer than n days will have their script validated during sync.
static const int DEFAULT_CHECKPOINT_DAYS = 30;
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
std::unique_ptr<ArgReset> SetDummyArgGetter(std::unique_ptr<ArgGetter>);

#endif
