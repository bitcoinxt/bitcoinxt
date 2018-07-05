#ifndef BITCOIN_LMDB_LMDBUTIL
#define BITCOIN_LMDB_LMDBUTIL
#include "config/bitcoin-config.h" // for USE_LMDB
#ifdef USE_LMDB
namespace lmdb {
void HandleError(const int status, const char* call, const char *file,
        const int line, const char* func);
} // ns lmdb

#define LMDB_RC_CHECK(x) lmdb::HandleError(x, #x, __FILE__, __LINE__, __func__)

#endif // USE_LMDB
#endif // BITCOIN_LMDB_LMDBUTIL
