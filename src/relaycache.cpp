#include "relaycache.h"
#include "utiltime.h"

RelayCache& RelayCache::Instance() {
    static RelayCache cache;
    return cache;
}

void RelayCache::ExpireOld() {
    std::unique_lock<std::mutex> lock(cs);
    if (expiration.empty()) {
        return;
    }
    int64_t time = GetTime();
    while (expiration.front().first <= time) {
        txs.erase(expiration.front().second);
        expiration.pop_front();
    }
}

int64_t RelayCache::GetTime() {
    return ::GetTime();
}

void RelayCache::RemoveExpiration(const uint256& txid) {
    for (auto e = begin(expiration); e != end(expiration); ++e) {
        if (e->second != txid) {
            continue;
        }
        expiration.erase(e);
        return;
    }
}

void RelayCache::Insert(const CTransaction& tx) {
    std::unique_lock<std::mutex> lock(cs);
    auto res = txs.insert({ tx.GetHash(), tx });
    if (!res.second) {
        // already existed, remove earlier expiration
        RemoveExpiration(tx.GetHash());
    }
    expiration.push_back({ GetTime() + RELAY_CACHE_TIMEOUT, tx.GetHash() });
}

CTransaction RelayCache::FindTx(const uint256& hash) {
    std::unique_lock<std::mutex> lock(cs);
    auto m = txs.find(hash);
    if (m == end(txs)) {
        static CTransaction NULL_TX;
        assert(NULL_TX.IsNull());
        return NULL_TX;
    }
    return m->second;
}
