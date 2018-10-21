#ifndef BITCOIN_DBRESIZE_MANAGER
#define BITCOIN_DBRESIZE_MANAGER
#include "config/bitcoin-config.h" // for USE_LMDB
#ifdef USE_LMDB

#include <mutex>
#include <functional>

typedef struct MDB_env MDB_env;
typedef struct MDB_txn MDB_txn;
typedef unsigned int MDB_dbi;

namespace lmdb {

// LMDB can only be resized when there are no transactions active. This class
// is responsible to increase the mapsize at appropriate time.
//
// To achive this, it wraps txn operations so it can track if any are active.
//
// This is a helper class for LMDBWrapper
class DBResizeManager {
public:
    DBResizeManager(const std::function<bool(uint64_t)> checkDiskSpace);
    void txn_begin(MDB_env* env, MDB_txn** txn, unsigned int flags = 0);
    void txn_renew(MDB_txn* txn);
    void txn_abort(MDB_txn* txn);
    void txn_commit(MDB_txn* txn);
    void txn_reset(MDB_txn* txn);

    void ResizeIfNeeded(MDB_env* env);

private:
    std::mutex cs;
    int txns_active = 0;
    //! function to check if there is enough disk space to resize the db.
    std::function<bool(uint64_t)> checkDiskSpace;

    bool NeedResize(uint64_t used, uint64_t old_max);
};

} // ns lmdb

#endif // USE_LMDB
#endif // BITCOIN_DBRESIZE_MANAGER
