#include "mempoolaccepter.h"
#include "amount.h"
#include "policy/txpriority.h"
#include "primitives/transaction.h"
#include "txmempool.h"
#include "util.h"
#include "utilprocessmsg.h"

static std::string TxIDStr(const CTxMemPoolEntry& entry) {
    return entry.GetTx().ToString();
}


static bool CheckFreeRateLimit(const CTxMemPoolEntry& entry,
                               RateLimitState* limit)
{
    assert(limit);
    std::unique_lock<std::mutex> lock(limit->cs);
    if (!RateLimitExceeded(limit->count, limit->lastTime,
                           limit->limit, entry.GetTxSize()))
    {
        LogPrint(Log::MEMPOOL, "Rate limit count: %g => %g\n",
                 limit->count, limit->count + entry.GetTxSize());
        return true;
    }

    return false;
}

FeeEvaluator::FeeEvaluator(bool allowFreeTxs,
                           const MempoolFeeModifier& feemodifier,
                           const CFeeRate& minRelayRate,
                           RateLimitState* limiter)
    : allowFreeTxs(allowFreeTxs), feemodifier(feemodifier), minRelayRate(minRelayRate)
{
    static RateLimitState defaultLimiter(GetArg("-limitfreerelay", 15));
    ratelimiter = limiter == nullptr ? &defaultLimiter : limiter;
}

FeeEvaluator::FeeState FeeEvaluator::HasSufficientFee(const CCoinsViewCache& view,
                                                      const CTxMemPoolEntry& entry,
                                                      int chainHeight) const
{
    if (chainHeight == -1)
        throw std::invalid_argument("invalid chainheight");

    const CTransaction& tx = entry.GetTx();
    if (feemodifier.GetDelta(tx.GetHash()) > 0) {
        // Any transaction that has had its fee artifically bumped doesn't
        // gets to pass. No need to consider fee/priority.
        return FEE_OK;
    }

    const size_t txSize = entry.GetTxSize();
    const CAmount& minFee = minRelayRate.GetFee(txSize);

    if (entry.GetFee() >= minFee) {
        return FEE_OK;
    }

    if (!IsPriorityCandidate(txSize)) {
        LogPrint(Log::MEMPOOL, "%s: not enough fees %s, %d < %d\n",
                 __func__, TxIDStr(entry), entry.GetFee(), minFee);

        return INSUFFICIENT_FEE;
    }

    double priority = GetPriority(view, entry, chainHeight);
    if (priority < AllowFreeThreshold()) {
        LogPrint(Log::MEMPOOL, "%s: insufficient priority %s, %d < %d\n",
                 __func__, TxIDStr(entry), priority, AllowFreeThreshold());

        return INSUFFICIENT_PRIORITY;
    }

    // Continuously rate-limit free (really, very-low-fee) transactions
    // This mitigates 'penny-flooding' -- sending thousands of free transactions just to
    // be annoying or make others' transactions take longer to confirm.
    if (!CheckFreeRateLimit(entry, ratelimiter)) {

        LogPrint(Log::MEMPOOL, "%s: %s rejected by rate limiter\n",
                 __func__, TxIDStr(entry));
        return RATE_LIMITED;
    }

    return FEE_OK;
}

bool FeeEvaluator::IsPriorityCandidate(size_t txSize) const {
    return allowFreeTxs && txSize <= MAX_FREE_TX_SIZE;
}

double FeeEvaluator::GetPriority(const CCoinsViewCache& view,
                                 const CTxMemPoolEntry& entry,
                                 int tipHeight) const
{
    // Free transactions must have sufficient priority to be mined in the next block.
    return ::GetPriority(view, entry.GetTx(), tipHeight + 1, entry.GetTxSize());
}

std::string FeeEvaluator::ToString(FeeState s) {
    switch (s) {
    case FeeState::INSUFFICIENT_FEE: return "insufficient fee";
    case FeeState::INSUFFICIENT_PRIORITY: return "insufficient priority";
    case FeeState::RATE_LIMITED: return "free tx rejected by rate limiter";
    case FeeState::FEE_OK: return "";
    default: return "unknown";
    }
}
