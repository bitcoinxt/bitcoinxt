#include "lmdb/dbresizemanager.h"
#ifdef USE_LMDB
#include "dbwrapper.h" // for dbwrapper_error
#include "lmdb/lmdbutil.h"
#include "util.h"

#include <lmdb.h>

namespace lmdb {

static const uint64_t ONE_GB = 1 * 1024 * 1024 * 1024;

DBResizeManager::DBResizeManager(const std::function<bool(uint64_t)> checkDiskSpace) :
    checkDiskSpace(checkDiskSpace)
{
}

void DBResizeManager::txn_begin(MDB_env* env, MDB_txn** txn, unsigned int flags) {
    std::unique_lock<std::mutex> lock(cs);
    LMDB_RC_CHECK(mdb_txn_begin(env, nullptr, flags, txn));
    ++txns_active;
}

void DBResizeManager::txn_renew(MDB_txn* txn) {
    std::unique_lock<std::mutex> lock(cs);
    LMDB_RC_CHECK(mdb_txn_renew(txn));
    ++txns_active;
}

void DBResizeManager::txn_abort(MDB_txn* txn) {
    std::unique_lock<std::mutex> lock(cs);
    mdb_txn_abort(txn);

    --txns_active;
    assert(txns_active >= 0);
}

void DBResizeManager::txn_commit(MDB_txn* txn) {
    std::unique_lock<std::mutex> lock(cs);
    LMDB_RC_CHECK(mdb_txn_commit(txn));

    --txns_active;
    assert(txns_active >= 0);
}

void DBResizeManager::txn_reset(MDB_txn* txn) {
    std::unique_lock<std::mutex> lock(cs);
    mdb_txn_reset(txn);

    --txns_active;
    assert(txns_active >= 0);
}

void DBResizeManager::ResizeIfNeeded(MDB_env* env) {
    std::unique_lock<std::mutex> lock(cs);
    MDB_stat stat;
    MDB_envinfo info;
    LMDB_RC_CHECK(mdb_env_stat(env, &stat));
    LMDB_RC_CHECK(mdb_env_info(env, &info));

    const uint64_t old_max = info.me_mapsize;
    const uint64_t used = stat.ms_psize * info.me_last_pgno;

    const bool need_resize = NeedResize(used, old_max);
    LogPrint(Log::DB, "lmdb: mapsize: %d, used %d, need resize %s\n",
        old_max, used, need_resize ? "yes" : "no");

    if (!need_resize) {
        return;
    }

    if (txns_active) {
        // For now we don't support parallel read/write to the database
        // and this method is called only when we know thee are no active
        // transactions.
        //
        // In the future, we may wait for transactions to finish at this
        // point.
        std::stringstream ss;
        ss << "Bad internal state - db needs resize, but " << txns_active
           << " txns are active.";
        throw dbwrapper_error(ss.str());
    }

    if (!checkDiskSpace(ONE_GB)) {
        throw dbwrapper_error("Out of disk space! Unable to resize database.");
    }

    // Bump max size.
    uint64_t new_max = old_max + ONE_GB;
    LMDB_RC_CHECK(mdb_env_set_mapsize(env, new_max));

    LogPrintf("lmdb: Max size increased from %d to %d\n", old_max, new_max);
}

bool DBResizeManager::NeedResize(uint64_t used, uint64_t old_max) {
    if (old_max < ONE_GB) {
        return true;
    }
    if (used * (10.0/8) > old_max) {
        return true;
    }
    return false;
}

} // ns lmdb

#endif // USE_LMDB
