#include "lmdb/lmdbwrapper.h"
#ifdef USE_LMDB
#include "dbwrapper.h"
#include "lmdb/dbresizemanager.h"
#include "lmdb/lmdbutil.h"
#include "util.h"

#include <lmdb.h>
#include <boost/filesystem.hpp>
#include <mutex>
#include <memory>

namespace lmdb {
namespace {

MDB_val to_mdbval(const CDataStream& s) {
    MDB_val v;
    v.mv_data = const_cast<char*>(s.data());
    v.mv_size = s.size();
    return v;
}

void WipeLMDB(const boost::filesystem::path& path) {
    LogPrintf("Wiping LMDB in %s\n", path.string());
    boost::filesystem::remove(path / "data.mdb");
    boost::filesystem::remove(path / "lock.mdb");
    try {
        boost::filesystem::remove(path);
    } catch(boost::filesystem::filesystem_error &e) {
        // Raised if the directory is not empty - we only want to remove the
        // directory if empty so that's OK.
    }
}

class LMDBBatch : public CDBBatch {
public:
    LMDBBatch(DBResizeManager& resizemg, MDB_env* env, MDB_dbi* dbi) :
        resizemg(resizemg), txn(nullptr), dbi(dbi), env(env), size_estimate(0)
    {
        resizemg.txn_begin(env, &txn);
    }

    ~LMDBBatch() {
        if (txn == nullptr)
            return;

        LogPrint(Log::DB, "%s aborting uncommited txn\n", __func__);
        resizemg.txn_abort(txn);
        txn = nullptr;
    }

    void Clear() override {
        size_estimate = 0;
        if (txn != nullptr) {
            LogPrint(Log::DB, "%s uncommited txn cleared\n", __func__);
            resizemg.txn_abort(txn);
            txn = nullptr;
        }
        resizemg.txn_begin(env, &txn);
    }

    size_t SizeEstimate() const override {
        return size_estimate;
    }

    void CommitTxn() {
        resizemg.txn_commit(txn);
        txn = nullptr;

        // after modification, resize if db is reaching its map size limit
        resizemg.ResizeIfNeeded(env);
    }

protected:
    void Write(const CDataStream& key, const CDataStream& value) override {
        assert(txn);
        MDB_val slKey(to_mdbval(key));
        MDB_val slValue(to_mdbval(value));

        LMDB_RC_CHECK(mdb_put(txn, *dbi, &slKey, &slValue, 0));

        // TODO: Investigate how LDMB serializes writes to give a more accurate estimate.
        size_estimate += key.size() + value.size();
    }

    void Erase(const CDataStream& key) override {
        assert(txn);
        MDB_val slKey(to_mdbval(key));
        int status = mdb_del(txn, *dbi, &slKey, NULL);
        if (status != MDB_NOTFOUND) {
            // ok to delete a non-existing key
            LMDB_RC_CHECK(status);
        }
        // TODO: Investigate how LDMB serializes writes to give a more accurate estimate.
        size_estimate += key.size();
    }

private:
    DBResizeManager& resizemg;
    MDB_txn* txn;
    MDB_dbi* dbi;
    MDB_env* env;
    size_t size_estimate;

    LMDBBatch(const LMDBBatch&) = delete;
    LMDBBatch& operator=(const LMDBBatch&) = delete;
};

class LMDBIterator : public CDBIterator {
public:
    LMDBIterator(MDB_cursor* cursor) : cursor(cursor), valid(false) {
    }
    ~LMDBIterator() {
        mdb_cursor_close(cursor);
    }

    bool Valid() override { return valid; }

    void SeekToFirst() override {
        int status = mdb_cursor_get(cursor, &key, &value, MDB_FIRST);
        if (status != MDB_NOTFOUND) {
            LMDB_RC_CHECK(status);
        }
        valid = status == MDB_SUCCESS;
    }

    void Seek(const CDataStream& ssKey) override {
        key.mv_data = const_cast<char*>(ssKey.data());
        key.mv_size = ssKey.size();
        int status = mdb_cursor_get(cursor, &key, &value, MDB_SET_RANGE);
        if (status != MDB_NOTFOUND) {
            LMDB_RC_CHECK(status);
        }
        valid = status == MDB_SUCCESS;
    }

    void Next() override {
        int status = mdb_cursor_get(cursor, &key, &value, MDB_NEXT);
        if (status != MDB_NOTFOUND) {
            LMDB_RC_CHECK(status);
        }
        valid = status == MDB_SUCCESS;
    }

    CDataStream GetKey() override {
        assert(valid);
        return CDataStream(static_cast<const char*>(key.mv_data),
                           static_cast<const char*>(key.mv_data) + key.mv_size,
                           SER_DISK, CLIENT_VERSION);
    }

    unsigned int GetKeySize() override {
        assert(valid);
        return key.mv_size;
    }

    CDataStream GetValue() override {
        assert(valid);
        return CDataStream(static_cast<const char*>(value.mv_data),
                           static_cast<const char*>(value.mv_data) + value.mv_size,
                           SER_DISK, CLIENT_VERSION);
    }

    unsigned int GetValueSize() override {
        assert(valid);
        return value.mv_size;
    }

private:
    MDB_cursor* cursor;
    bool valid;
    MDB_val key;
    MDB_val value;
    LMDBIterator(const LMDBIterator&) = delete;
    LMDBIterator& operator=(const LMDBIterator&) = delete;
};

class LMDBWrapper : public CDBWrapper {
public:
    LMDBWrapper(const boost::filesystem::path& path, bool fMemory, bool fWipe, bool fSafeMode)
        : wipe_on_close(fMemory)
    {
        LogPrintf("Opening LMDB in %s. Version %s. Safe mode %s\n",
                  path.string(), MDB_VERSION_STRING, fSafeMode ? "yes" : "no");

        LMDB_RC_CHECK(mdb_env_create(&env));

        unsigned int flags = 0;
        if (fMemory) {
            // no need to sync, db will be wiped anyway.
            flags |= MDB_NOSYNC | MDB_WRITEMAP;
        }
        else {
            // Disable readahead, which can be harmful to random read
            // performance when available system RAM is less than DB size.
            flags |= MDB_NORDAHEAD;

            // By default, LMDB forces the OS to flush after every
            // commit. Disabling this gives performance boost, but makes the DB
            // more likely to corrupt on abnormal program termination.
            if (!fSafeMode)
                flags |= MDB_NOSYNC;
        }
        if (fWipe) {
            WipeLMDB(path);
        }

        boost::filesystem::create_directories(path);
        LMDB_RC_CHECK(mdb_env_open(env, path.string().c_str(), flags, 0644));

        MDB_envinfo info;
        mdb_env_info(env, &info);
        // Open database
        auto diskSpaceCheck = [path](uint64_t needed) {
            return boost::filesystem::space(path).available >= needed;
        };
        resizemg.reset(new DBResizeManager(diskSpaceCheck));
        MDB_txn* txn;
        resizemg->txn_begin(env, &txn);
        LMDB_RC_CHECK(mdb_dbi_open(txn, nullptr, 0, &dbi));
        resizemg->txn_commit(txn);
        resizemg->ResizeIfNeeded(env);

        // Create a reusable read txn. It must be renewed after every write.
        resizemg->txn_begin(env, &rd_txn, MDB_RDONLY);
        LogPrintf("Opened LMDB database successfully\n");
    }

    ~LMDBWrapper() {
        boost::filesystem::path path;
        const char* cpath;
        if (wipe_on_close && mdb_env_get_path(env, &cpath) == MDB_SUCCESS) {
            path = cpath;
        }
        if (rd_txn != nullptr) {
            resizemg->txn_abort(rd_txn);
        }
        if (env != nullptr) {
            mdb_env_close(env);
        }
        if (!path.empty()) {
            assert(wipe_on_close);
            try {
                WipeLMDB(path);
            }
            catch (...) {
                LogPrintf("%s - WipeLMDB threw\n", __func__);
            }
        }
    }

    bool WriteBatch(CDBBatch& batch, bool fSync = false) override {
        // reset readtxn before write
        resizemg->txn_reset(rd_txn);

        LMDBBatch* lbatch = dynamic_cast<LMDBBatch*>(&batch);
        lbatch->CommitTxn();
        if (fSync) {
            Sync();
        }
        // renew after write
        resizemg->txn_renew(rd_txn);
        return true;
    }

    // not supported by LMDB
    bool Flush() override {
        return true;
    }

    bool Sync() override {
        int forceSync = 1;
        LMDB_RC_CHECK(mdb_env_sync(env, forceSync));
        return true;
    }

    CDBIterator* NewIterator() override {
        MDB_cursor* cursor;
        LMDB_RC_CHECK(mdb_cursor_open(rd_txn, dbi, &cursor));
        return new LMDBIterator(cursor);
    }

    std::unique_ptr<CDBBatch> NewBatch() override {
        return std::unique_ptr<CDBBatch>(new LMDBBatch(*resizemg, env, &dbi));
    }

protected:
    CDataStream Read(const CDataStream& key) const override {
        MDB_val slKey(to_mdbval(key));
        MDB_val value;
        int status = mdb_get(rd_txn, dbi, &slKey, &value);
        if (status == MDB_NOTFOUND) {
            throw dbwrapper_notfound{};
        }
        LMDB_RC_CHECK(status);
        return CDataStream(static_cast<const char*>(value.mv_data),
                           static_cast<const char*>(value.mv_data) + value.mv_size,
                           SER_DISK, CLIENT_VERSION);
    }

    bool Exists(const CDataStream& key) const override {
        MDB_val slKey(to_mdbval(key));
        MDB_val value;
        int status = mdb_get(rd_txn, dbi, &slKey, &value);
        if (status == MDB_NOTFOUND) {
            return false;
        }
        LMDB_RC_CHECK(status);
        return true;
    }

    size_t EstimateSize() const {
        MDB_stat stat;
        MDB_envinfo info;
        LMDB_RC_CHECK(mdb_env_stat(env, &stat));
        LMDB_RC_CHECK(mdb_env_info(env, &info));
        return stat.ms_psize * info.me_last_pgno;
    }

private:
    std::unique_ptr<DBResizeManager> resizemg;
    MDB_env* env;
    MDB_dbi dbi;

    //! reusable transaction for reading
    MDB_txn* rd_txn;

    bool wipe_on_close;
};

} // ns anon

std::unique_ptr<CDBWrapper> CreateLMDB(
        const boost::filesystem::path& path,
        bool fMemory, bool fWipe, bool fSafeMode) {
    return std::unique_ptr<CDBWrapper>(new LMDBWrapper(path, fMemory, fWipe, fSafeMode));
}
} // ns lmdb

#endif // USE_LMDB
