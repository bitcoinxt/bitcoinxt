#include "options.h"
#include "util.h"

static std::auto_ptr<ArgGetter> Args;

struct DefaultGetter : public ArgGetter {
    virtual bool GetBool(const std::string& arg, bool def) {
        return ::GetBoolArg(arg, def);
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


std::auto_ptr<ArgReset> SetDummyArgGetter(std::auto_ptr<ArgGetter> getter) {
    Args.reset(getter.release());
    return std::auto_ptr<ArgReset>(new ArgReset);
}

ArgReset::~ArgReset() {
    Args.reset(new DefaultGetter());
}

