#ifndef BITCOIN_OPTIONS_H
#define BITCOIN_OPTIONS_H

#include <cstdint>
#include <memory>
#include <map>
#include <string>
#include <vector>

class Opt {
public:
    Opt();

    // User agent options
    std::string UserAgent() const;
    bool HidePlatform();
    std::vector<std::string> UAComment(bool validate = false) const;

        int ScriptCheckThreads();
        int64_t CheckpointDays();
        uint64_t MaxBlockSizeVote();
        int64_t RespendRelayLimit() const;
	bool UseCashAddr() const;

    // Fork activation
    int64_t UAHFTime() const;
    int UAHFProtectSunset();
    int64_t ThirdHFTime() const;
    int64_t FourthHFTime() const;

        // Thin block options
        bool UsingThinBlocks();
        bool AvoidFullBlocks();
        int ThinBlocksMaxParallel();
        bool PreferXThinBlocks() const;

    // Policy
    bool AllowFreeTx() const;

    //! Throws invalid argument on use of options that are no longer supported.
    void CheckRemovedOptions() const;
};

/** Maximum number of script-checking threads allowed */
static const int MAX_SCRIPTCHECK_THREADS = 16;
/** -par default (number of script-checking threads, 0 = auto) */
static const int DEFAULT_SCRIPTCHECK_THREADS = 0;
// Blocks newer than n days will have their script validated during sync.
static const int DEFAULT_CHECKPOINT_DAYS = 30;
/** User-activated hard fork default activation time */
static const int64_t UAHF_DEFAULT_ACTIVATION_TIME = 1501590000; // Tue 1 Aug 2017 12:20:00 UTC
/** User-activated hard fork protect-this-chain-from-replay sunset height */
static const int UAHF_DEFAULT_PROTECT_THIS_SUNSET = 530000;

//
// For unit testing
//

class ArgGetter {
    public:
        virtual ~ArgGetter() = 0;
        virtual bool GetBool(const std::string& arg, bool def) = 0;
        virtual std::vector<std::string> GetMultiArgs(const std::string& arg) = 0;
        virtual int64_t GetArg(const std::string& arg, int64_t def) = 0;
        virtual std::string GetArg(const std::string& arg,
                                   const std::string& def) = 0;
};
inline ArgGetter::~ArgGetter() { }

struct ArgReset {
    ~ArgReset();
};

// Temporary replace the global getter for fetching user configurations.
// Returns a RAII object that sets system back to default state.
std::unique_ptr<ArgReset> SetDummyArgGetter(std::unique_ptr<ArgGetter>);

// Reusable dummy class for simple use cases.
// Example usage:
//     auto arg = new DummyArgGetter;
//     auto argraii = SetDummyArgGetter(std::unique_ptr<ArgGetter>(arg));
//     arg->Set("-maxblocksizevote", 64);
class DummyArgGetter : public ArgGetter {
    public:
        bool GetBool(const std::string& arg, bool def) override;
        std::vector<std::string> GetMultiArgs(const std::string& arg) override;
        int64_t GetArg(const std::string& str, int64_t def) override;
        std::string GetArg(const std::string& str, const std::string& def) override;

        void Set(const std::string& arg, int64_t val);
        void Set(const std::string& arg, const std::string& val);

    private:
        std::map<std::string, std::string> customArgs;
};

#endif
