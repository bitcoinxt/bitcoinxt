// Copyright (c) 2018 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "respend/mempoolremover.h"

namespace respend {

MempoolRemover::MempoolRemover(CTxMemPool& pool, std::list<CTransaction>& removed) :
        pool(pool), removed(removed), valid(false)
{
}

bool MempoolRemover::AddOutpointConflict(
        const COutPoint&, const CTxMemPool::txiter mempoolEntry,
        const CTransaction& respendTx,
        bool seenBefore, bool isEquivalent)
{
    tx1s.insert(mempoolEntry);

    // Keep gathering conflicting transactions
    return true;
}

bool MempoolRemover::IsInteresting() const {
    return true;
}

void MempoolRemover::Trigger() {
    if (valid) {
        for (const auto& tx1Entry : tx1s)
            pool.removeRecursive(tx1Entry->GetTx(), removed);
    }
}

} // ns respend
