#ifndef BITCOIN_RELAYCACHE_H
#define BITCOIN_RELAYCACHE_H

#include <unordered_map>
#include <mutex>
#include <deque>

#include "primitives/transaction.h"
#include "uint256.h"
#include "utilhash.h"

class CTransaction;

constexpr int64_t RELAY_CACHE_TIMEOUT = 15 * 60;

class RelayCache {
public:

    static RelayCache& Instance();

    void Insert(const CTransaction& tx);
    CTransaction FindTx(const uint256& hash);

    void ExpireOld();

protected: // for testing
    RelayCache() { }
    virtual int64_t GetTime();

private:
    RelayCache(const RelayCache&) = delete;
    void RemoveExpiration(const uint256& txid);

    std::unordered_map<uint256, CTransaction, SaltedTxIDHasher> txs;
    std::deque<std::pair<int64_t, uint256> > expiration;
    mutable std::mutex cs;
};

#endif
