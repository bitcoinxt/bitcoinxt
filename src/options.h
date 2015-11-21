#ifndef BITCOIN_OPTIONS_H
#define BITCOIN_OPTIONS_H

#include <string>
#include <memory>

struct Opt {
    Opt();

    bool IsStealthMode();
    bool HidePlatform();

};

//
// For unit testing
//

struct ArgGetter {
    virtual bool GetBool(const std::string& arg, bool def) = 0;
};

struct ArgReset {
    ~ArgReset();
};

// Temporary replace the global getter for fetching user configurations.
//
// Returns a RAII object that sets system back to default state.
std::auto_ptr<ArgReset> SetDummyArgGetter(std::auto_ptr<ArgGetter>);

#endif
