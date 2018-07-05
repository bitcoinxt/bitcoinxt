#include "config/bitcoin-config.h" // for USE_LMDB
#ifdef USE_LMDB
#include "test/test_bitcoin.h"
#include "lmdb/dbresizemanager.h"
#include "dbwrapper.h"
#include <boost/test/unit_test.hpp>
#include <lmdb.h>

class DBResizeFixture : public TestingSetup {
public:
    DBResizeFixture() : TestingSetup() {
        // create db env and open db
        int rc;
        rc = mdb_env_create(&env);
        assert(rc == MDB_SUCCESS);
        rc = mdb_env_open(env, pathTemp.string().c_str(), MDB_NOSYNC | MDB_WRITEMAP, 0664);
        assert(rc == MDB_SUCCESS);
        MDB_txn* txn;
        rc = mdb_txn_begin(env, nullptr, 0, &txn);
        assert(rc == MDB_SUCCESS);
        rc = mdb_dbi_open(txn, nullptr, 0, &dbi);
        assert(rc == MDB_SUCCESS);
        rc = mdb_txn_commit(txn);
        assert(rc == MDB_SUCCESS);

        diskSpaceOK = [](uint64_t) { return true; };
        lowDiskSpace = [](uint64_t) { return false; };
    }
    ~DBResizeFixture() {
        mdb_dbi_close(env, dbi);
        mdb_env_close(env);
    }
    uint64_t mapsize() {
        MDB_envinfo info;
        int rc = mdb_env_info(env, &info);
        assert(rc == MDB_SUCCESS);
        return info.me_mapsize;
    }
    MDB_env* env;
    MDB_dbi dbi;
    std::function<bool(uint64_t)> diskSpaceOK;
    std::function<bool(uint64_t)> lowDiskSpace;
};

BOOST_FIXTURE_TEST_SUITE(dbresizemanager_tests, DBResizeFixture);

BOOST_AUTO_TEST_CASE(dbresize_ok) {
    // lmdb default mapsize is low, so resize is needed.
    uint64_t oldsize = mapsize();
    lmdb::DBResizeManager mg(diskSpaceOK);
    mg.ResizeIfNeeded(env);
    assert(mapsize() > oldsize);

    // already resized, no resize needed
    oldsize = mapsize();
    mg.ResizeIfNeeded(env);
    assert(oldsize == mapsize());
}

BOOST_AUTO_TEST_CASE(throws_on_active_tx1) {
    lmdb::DBResizeManager mg(diskSpaceOK);
    MDB_txn* txn;
    mg.txn_begin(env, &txn);
    BOOST_CHECK_THROW(mg.ResizeIfNeeded(env), dbwrapper_error);
    mg.txn_commit(txn);
    BOOST_CHECK_NO_THROW(mg.ResizeIfNeeded(env));
}

BOOST_AUTO_TEST_CASE(throws_on_active_tx2) {
    lmdb::DBResizeManager mg(diskSpaceOK);
    MDB_txn* txn;
    mg.txn_begin(env, &txn);
    BOOST_CHECK_THROW(mg.ResizeIfNeeded(env), dbwrapper_error);
    mg.txn_commit(txn);
    BOOST_CHECK_NO_THROW(mg.ResizeIfNeeded(env));
}

BOOST_AUTO_TEST_CASE(throws_on_active_tx3) {
    lmdb::DBResizeManager mg(diskSpaceOK);
    MDB_txn* txn;
    mg.txn_begin(env, &txn, MDB_RDONLY);
    mg.txn_reset(txn);
    mg.txn_renew(txn);
    BOOST_CHECK_THROW(mg.ResizeIfNeeded(env), dbwrapper_error);
    mg.txn_abort(txn);
    BOOST_CHECK_NO_THROW(mg.ResizeIfNeeded(env));
}

BOOST_AUTO_TEST_SUITE_END();

#endif
