// Copyright (c) 2018 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "respend/mempoolremover.h"
#include "policy/policy.h"
#include "util.h"

#include <algorithm>

namespace respend {

MempoolRemover::MempoolRemover(RemoveWhat toRemove,
                               std::list<CTransaction>* removed) :
        toRemove(toRemove), removed(removed), canReplace(false),
        valid(false)
{
    if (this->removed == nullptr)
        this->removed = &dummy;
}

bool MempoolRemover::AddOutpointConflict(const COutPoint&,
        const CTxMemPool::txiter, const CTransaction& respendTx,
        bool seenBefore, bool isEquivalent, bool isSICandidate)
{
    switch (toRemove)
    {
        case REMOVE_ALL:
        {
            return true;
        }
        case REMOVE_NON_SI:
        {
            if (isEquivalent)
                return true;
            canReplace = IsSuperStandardImmediateTx(respendTx, isSICandidate);
            break;
        }
        default:
        {
            LogPrint(Log::RESPEND, "respend: undefined RemoveWhat value %i\n", toRemove);
            break;
        }
    }
    return false;
}


bool MempoolRemover::IsInteresting() const {
    // A remover never triggers full tx validation on its own
    return false;
}

void MempoolRemover::OnValidTrigger(bool v, CTxMemPool& pool,
        CTxMemPool::setEntries& conflictingEntries)
{
    valid = v;
    if (!valid)
        return;

    switch (toRemove)
    {
        case REMOVE_ALL:
        {
            for (const auto& entry : conflictingEntries) {
                pool.removeRecursive(entry->GetTx(), *removed);
            }
            break;
        }
        case REMOVE_NON_SI:
        {
            if (!canReplace || conflictingEntries.size() != 1)
                return;

            CTxMemPool::txiter entry = *conflictingEntries.begin();
            if (!entry->IsLiveSI()) {
                pool.removeRecursive(entry->GetTx(), *removed);
                conflictingEntries.erase(entry);
            }
            break;
        }
        default:
        {
            LogPrint(Log::RESPEND, "respend: undefined RemoveWhat value %i\n", toRemove);
            break;
        }
    }
}

} // ns respend
