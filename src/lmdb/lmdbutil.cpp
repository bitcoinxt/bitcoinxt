#include "lmdb/lmdbutil.h"
#ifdef USE_LMDB

#include "dbwrapper.h" // for dbwrapper_error
#include "util.h"

#include <lmdb.h>

#include <sstream>

namespace lmdb {

void HandleError(const int status, const char* call, const char *file,
        const int line, const char* func)
{
    if (status == MDB_SUCCESS)
        return;

    std::stringstream err;
    err << mdb_strerror(status) << " (code: " << status << ")."
        << " Call: '" << call
        << "' in function '" << func << "'"
        << " file " << file << ":" << line;
    LogPrintf("lmdb error: %s\n", err.str());
    throw dbwrapper_error(err.str());
}

} // ns lmdb

#endif // USE_LMDB
