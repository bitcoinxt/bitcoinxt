#ifndef BITCOIN_UTILDEBUG_H
#define BITCOIN_UTILDEBUG_H

#include <stdexcept>

void dump_backtrace_stderr();

class bad_state_error : public std::runtime_error {
    using std::runtime_error::runtime_error; // inherit constructors
};

void throw_bad_state(const char* why,
                            const char* func, const char* file, int line);

// Can be used instead of assert where we are able to recover from an exception.
#define THROW_UNLESS(cond) { if (!(cond)) { throw_bad_state(#cond, __func__, __FILE__, __LINE__); } }

#endif
