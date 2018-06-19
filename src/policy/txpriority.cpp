#include "policy/txpriority.h"
#include "primitives/transaction.h"
#include "coins.h"

// Compute modified tx size for priority calculation
static size_t CalculateModifiedSize(const CTransaction& tx, size_t nTxSize)
{
    // In order to avoid disincentivizing cleaning up the UTXO set we don't count
    // the constant overhead for each txin and up to 110 bytes of scriptSig (which
    // is enough to cover a compressed pubkey p2sh redemption) for priority.
    // Providing any more cleanup incentive than making additional inputs free would
    // risk encouraging people to create junk outputs to redeem later.
    for (auto& in : tx.vin)
    {
        size_t offset = size_t(41) + std::min(size_t(110), in.scriptSig.size());
        if (nTxSize > offset)
            nTxSize -= offset;
    }
    return nTxSize;
}

double ComputePriority(const CTransaction& tx, double dPriorityInputs, size_t nTxSize)
{
    nTxSize = CalculateModifiedSize(tx, nTxSize);
    if (nTxSize == 0) return 0.0;

    return dPriorityInputs / nTxSize;
}

double GetPriority(const CCoinsViewCache& view, const CTransaction &tx,
                   uint32_t nHeight, size_t nTxSize)
{
    if (tx.IsCoinBase())
        return 0.0;

    double dResult = 0.0;
    for (const CTxIn& txin : tx.vin)
    {
        const Coin& coin = view.AccessCoin(txin.prevout);
        if (coin.IsSpent()) continue;
        if (coin.nHeight < nHeight) {
            dResult += coin.out.nValue * (nHeight - coin.nHeight);
        }
    }

    if (nTxSize == 0)
        nTxSize = ::GetSerializeSize(tx, SER_NETWORK, PROTOCOL_VERSION);

    return ComputePriority(tx, dResult, nTxSize);
}
