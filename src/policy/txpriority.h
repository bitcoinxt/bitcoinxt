#ifndef BITCOIN_TXPRIORITY_H
#define BITCOIN_TXPRIORITY_H

#include "amount.h" // COIN

class CCoinsViewCache;
class CTransaction;

inline double AllowFreeThreshold()
{
    return COIN * 144 / 250;
}

inline bool AllowFree(double dPriority)
{
    // Large (in bytes) low-priority (new, small-coin) transactions
    // need a fee.
    return dPriority > AllowFreeThreshold();
}

//! Return priority of tx at height nHeight
double GetPriority(const CCoinsViewCache& view,
                   const CTransaction &tx, uint32_t nHeight,
                   size_t nTxSize = 0);

// Compute priority, given priority of inputs and tx size
double ComputePriority(const CTransaction& tx, double dPriorityInputs,
                       size_t nTxSize);

#endif // BITCOIN_PRIORITYTX_H
