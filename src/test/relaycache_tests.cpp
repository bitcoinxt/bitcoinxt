#include "relaycache.h"
#include "test/test_bitcoin.h"
#include <boost/test/unit_test.hpp>

namespace {

class DummyRelayCache : public RelayCache {
public:
    DummyRelayCache() : RelayCache(), time(0) {

    }
    virtual int64_t GetTime() override {
        return time;
    }
    int64_t time;
};

} // ns anon

BOOST_FIXTURE_TEST_SUITE(relaycache_tests, BasicTestingSetup);

BOOST_AUTO_TEST_CASE(test_insert) {
    DummyRelayCache cache;

    CMutableTransaction tx;
    tx.nVersion = 42;
    assert(!CTransaction(tx).GetHash().IsNull());
    cache.Insert(tx);
    BOOST_CHECK(cache.FindTx(tx.GetHash()) == tx);
}

BOOST_AUTO_TEST_CASE(test_expire) {
    DummyRelayCache cache;

    CMutableTransaction tx1;
    tx1.nVersion = 42;
    CMutableTransaction tx2;
    tx2.nVersion = 24;

    cache.time = 100; // insertion time
    cache.Insert(tx1);
    cache.time = 101;
    cache.Insert(tx2);

    BOOST_CHECK(cache.FindTx(tx1.GetHash()) == tx1);
    BOOST_CHECK(cache.FindTx(tx2.GetHash()) == tx2);

    // Expire tx1
    cache.time = 100 + RELAY_CACHE_TIMEOUT;
    BOOST_CHECK(cache.FindTx(tx1.GetHash()).IsNull());
    BOOST_CHECK(cache.FindTx(tx2.GetHash()) == tx2);

    // Expire tx2
    cache.time = 101 + RELAY_CACHE_TIMEOUT;
    BOOST_CHECK(cache.FindTx(tx2.GetHash()).IsNull());
}

BOOST_AUTO_TEST_CASE(test_reset_expire) {

    CMutableTransaction tx;
    tx.nVersion = 42;

    DummyRelayCache cache;
    cache.time = 100; // insertion time
    cache.Insert(tx);

    cache.time = 100 + RELAY_CACHE_TIMEOUT - 1;
    cache.Insert(tx);

    // The second insertion should have reset the expiration time, so tx should
    // not expire.
    cache.time = 100 + RELAY_CACHE_TIMEOUT;
    cache.ExpireOld();
    BOOST_CHECK(cache.FindTx(tx.GetHash()) == tx);
}

BOOST_AUTO_TEST_SUITE_END()
