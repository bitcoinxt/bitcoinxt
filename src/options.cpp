#include "options.h"
#include "util.h"
#include <stdexcept>

static std::auto_ptr<ArgGetter> Args;

struct DefaultGetter : public ArgGetter {
    virtual bool GetBool(const std::string& arg, bool def) {
        return ::GetBoolArg(arg, def);
    }
    virtual std::vector<std::string> GetMultiArgs(const std::string& arg) {
        if (!mapMultiArgs.count(arg))
            return std::vector<std::string>();

        return mapMultiArgs[arg];
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

std::auto_ptr<ArgReset> SetDummyArgGetter(std::auto_ptr<ArgGetter> getter) {
    Args.reset(getter.release());
    return std::auto_ptr<ArgReset>(new ArgReset);
}

ArgReset::~ArgReset() {
    Args.reset(new DefaultGetter());
}

