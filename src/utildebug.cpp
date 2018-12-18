#include "utildebug.h"
#include "util.h"

#ifdef __linux__
#include <execinfo.h>
#include <unistd.h>
#endif
#include <iostream>

void dump_backtrace_stderr() {
#ifdef __linux__
    void* trace[100];
    int nptrs = backtrace(trace, 100);
    fprintf(stderr, "\nBacktrace dump:\n");
    backtrace_symbols_fd(trace, nptrs, STDERR_FILENO);

    std::cerr << std::endl
              << "To translate addresses to symbols, start GDB (gdb ./bitcoind)\n"
              << "Then call 'info symbol <address>'\n"
              << "Example stacktrace:\n"
              << "\t./bitcoind(+0x39ae5)[0x56140c19dae5]\n"
              << "Example GDB output:\n"
              << "\t(gdb) info symbol +0x39ae5\n"
              << "\tAppInit(int, char**) + 2568 in section .text\n"
              << std::endl;
#else
    std::cerr << "Backtrace not available on this platform" << std::endl;
#endif
}

void throw_bad_state(const char* why, const char* func,
                            const char* file, int line)
{
    std::stringstream err;
    err << "Bad state - expectation '" << why << "' failed at  "
        << func << " in " << file << ":" << line;

    LogPrintf("ERROR: %s", err.str());
    throw bad_state_error(err.str());
}
