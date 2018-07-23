#include "mempoolfeemodifier.h"
#include "memusage.h"
#include "util.h"

CAmount MempoolFeeModifier::GetDelta(const uint256& txid) const {
    std::unique_lock<std::mutex> lock(cs);
    auto amount = deltas.find(txid);
    if (amount == end(deltas))
        return CAmount(0);
    return amount->second;
}

void MempoolFeeModifier::AddDelta(const uint256& txid, const CAmount& amount) {
    std::unique_lock<std::mutex> lock(cs);
    if (!deltas.count(txid)) {
        deltas[txid] = amount;
    }
    else {
        deltas[txid] += amount;
    }
    LogPrint(Log::MEMPOOL, "%s: %s fee += %d\n"
             , __func__, txid.ToString(), amount);
}

void MempoolFeeModifier::RemoveDelta(const uint256& txid) {
    std::unique_lock<std::mutex> lock(cs);
    deltas.erase(txid);
}

size_t MempoolFeeModifier::DynamicMemoryUsage() const {
    std::unique_lock<std::mutex> lock(cs);
    return memusage::DynamicUsage(deltas);
}
