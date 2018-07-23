#ifndef BITCOIN_MEMPOOLFEEMODIFIER_H
#define BITCOIN_MEMPOOLFEEMODIFIER_H

#include "utilhash.h" // Salteduint256Hasher

#include <mutex>
#include <unordered_map>

class uint256;
typedef int64_t CAmount;

/**
 * Artifically modify fees for selected transactions to increase/decrease their
 * chance of being included in a block.
 */
class MempoolFeeModifier {
public:

    CAmount GetDelta(const uint256& txid) const;
    void AddDelta(const uint256& txid, const CAmount& amount);
    void RemoveDelta(const uint256& txid);

    size_t DynamicMemoryUsage() const;

private:
    mutable std::mutex cs;
    std::unordered_map<uint256, CAmount, SaltedTxIDHasher> deltas;
};

#endif
