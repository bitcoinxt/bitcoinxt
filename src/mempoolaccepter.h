#ifndef BITCOIN_MEMPOOLACCEPTER_H
#define BITCOIN_MEMPOOLACCEPTER_H

#include <cstdint>
#include <mutex>

class CCoinsViewCache;
class CFeeRate;
class CTxMemPoolEntry;
class MempoolFeeModifier;

typedef int64_t CAmount;

/**
 * This was DEFAULT_BLOCK_PRIORITY_SIZE - 1000. The concept of block priority
 * size is gone.
 *
 * We want to keep this limit large, as we don't want to encourage sending
 * muiltiple transactions instead of one big transaction for avoiding fees.
 */
static const uint64_t MAX_FREE_TX_SIZE = 49000;
/**
 * Fee is considered absurdly high if it's higher or equal to minimal relay fee
 * multiplied with this factor.
 */
static const int64_t ABSURD_FEE_FACTOR = 10000;

struct RateLimitState {
    RateLimitState(int64_t limit) : count(0), lastTime(0), limit(limit) {
    }
    double count;
    int64_t lastTime;
    int64_t limit;
    std::mutex cs;
};

// Evaluate if transaction has enough fee to enter the mempool.
// This class is thread safe.
class FeeEvaluator {
public:
    enum FeeState {
        FEE_OK,
        ABSURD_HIGH_FEE,
        INSUFFICIENT_FEE,
        INSUFFICIENT_PRIORITY,
        RATE_LIMITED
    };

    FeeEvaluator(bool allowFreeTxs, const MempoolFeeModifier& feemodifier,
                 const CFeeRate& minRelayRate, RateLimitState* limiter = nullptr);

    FeeState HasSufficientFee(const CCoinsViewCache& view,
                              const CTxMemPoolEntry& entry, int chainHeight) const;

    static std::string ToString(FeeState s);

protected:
    virtual double GetPriority(const CCoinsViewCache&,
                               const CTxMemPoolEntry&, int chainHeight) const;

private:
    const bool allowFreeTxs;
    const MempoolFeeModifier& feemodifier;
    const CFeeRate& minRelayRate;
    RateLimitState* ratelimiter;

    bool IsPriorityCandidate(size_t txSize) const;
    bool HasAbsurdFee(CAmount minRelayFee, CAmount txFee) const;
};

#endif
