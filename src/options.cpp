#include "chainparams.h"
#include "options.h"
#include "util.h"
#include <stdexcept>
#include <boost/thread.hpp>

static std::unique_ptr<ArgGetter> Args;

struct DefaultGetter : public ArgGetter {
    virtual bool GetBool(const std::string& arg, bool def) {
        return ::GetBoolArg(arg, def);
    }
    virtual std::vector<std::string> GetMultiArgs(const std::string& arg) {
        if (!mapMultiArgs.count(arg))
            return std::vector<std::string>();

        return mapMultiArgs[arg];
    }
    virtual int64_t GetArg(const std::string& strArg, int64_t nDefault) {
        return ::GetArg(strArg, nDefault);
    }
};

Opt::Opt() {
    if (!bool(Args.get()))
        Args.reset(new DefaultGetter());
}

bool Opt::IsStealthMode() {
    return Args->GetBool("-stealth-mode", false);
}

bool Opt::HidePlatform() {
    return Args->GetBool("-hide-platform", false);
}

std::vector<std::string> Opt::UAComment(bool validate) {
    std::vector<std::string> uacomments = Args->GetMultiArgs("-uacomment");
    if (!validate)
        return uacomments;

    typedef std::vector<std::string>::const_iterator auto_;
    for (auto_ c = uacomments.begin(); c != uacomments.end(); ++c) {
        size_t pos = c->find_first_of("/:()");
        if (pos == std::string::npos)
            continue;
        std::stringstream ss;
        ss << "-uacomment '" << *c << "' contains the reserved character '"
            << c->at(pos) << "'.The following characters are reserved: / : ( )";
        throw std::invalid_argument(ss.str());
    }

    return uacomments;
}

int Opt::ScriptCheckThreads() {
    // -par=0 means autodetect, but nScriptCheckThreads==0 means no concurrency
    int nScriptCheckThreads = Args->GetArg("-par", DEFAULT_SCRIPTCHECK_THREADS);
    if (nScriptCheckThreads <= 0) {
        static int numCores = GetNumCores();
        nScriptCheckThreads += numCores;
    }
    if (nScriptCheckThreads <= 1)
        nScriptCheckThreads = 0;
    else if (nScriptCheckThreads > MAX_SCRIPTCHECK_THREADS)
        nScriptCheckThreads = MAX_SCRIPTCHECK_THREADS;
    return nScriptCheckThreads;
}

int64_t Opt::CheckpointDays() {
    int64_t def = DEFAULT_CHECKPOINT_DAYS * std::max(1, ScriptCheckThreads());
    return std::max(int64_t(1), Args->GetArg("-checkpoint-days", def));
}

uint64_t Opt::MaxBlockSizeVote() {
    return Args->GetArg("-maxblocksizevote", 0);
}

int64_t Opt::UAHFTime() const {
    int64_t defaultUAHFTime = Params().NetworkIDString() == CBaseChainParams::REGTEST ?
                              Params().GenesisBlock().nTime :
                              UAHF_DEFAULT_ACTIVATION_TIME;

    return Args->GetArg("-uahftime", defaultUAHFTime);
}

int Opt::UAHFProtectSunset() {
    return Args->GetArg("-uahfprotectsunset", UAHF_DEFAULT_PROTECT_THIS_SUNSET);
}

// Activation time of the third BCH hard fork
int64_t Opt::ThirdHFTime() const {

    // Never activate if we're on the BTC chain
    if (!bool(UAHFTime()))
        return 0;

    // Default to Tuesday, May 15, 2018 4:00:00 PM
    const int64_t MTP_ACTIVATION = 0; // TODO: Set to 1526400000 in the final HF code
    return Args->GetArg("-thirdhftime", MTP_ACTIVATION);
}

int64_t Opt::RespendRelayLimit() const {
    return Args->GetArg("-limitrespendrelay", 100);
}

bool Opt::UseCashAddr() const {
    return Args->GetBool("-usecashaddr", bool(UAHFTime()));
}

bool Opt::UsingThinBlocks() {
    if (IsStealthMode())
        return false;
    return Args->GetBool("-use-thin-blocks", true);
}

/// Don't request blocks from nodes that don't support thin blocks.
bool Opt::AvoidFullBlocks() {
    return Args->GetArg("-use-thin-blocks", 1) == 2
        || Args->GetArg("-use-thin-blocks", 1) == 3;
}

int Opt::ThinBlocksMaxParallel() {
    return Args->GetArg("-thin-blocks-max-parallel", 3);
}

bool Opt::PreferXThinBlocks() const {
    return Args->GetBool("-prefer-xthin-blocks", false);
}

std::unique_ptr<ArgReset> SetDummyArgGetter(std::unique_ptr<ArgGetter> getter) {
    Args.reset(getter.release());
    return std::unique_ptr<ArgReset>(new ArgReset);
}

ArgReset::~ArgReset() {
    Args.reset(new DefaultGetter());
}

bool DummyArgGetter::GetBool(const std::string& arg, bool def) {
    return customArgs.count(arg) ? bool(customArgs[arg]) : def;
}

std::vector<std::string> DummyArgGetter::GetMultiArgs(const std::string&) {
    assert(!"not supported");
}

int64_t DummyArgGetter::GetArg(const std::string& arg, int64_t def) {
    return customArgs.count(arg) ? customArgs[arg] : def;
}

void DummyArgGetter::Set(const std::string& arg, int64_t val) {
    customArgs[arg] = val;
}
