// Copyright (c) 2018 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef BITCOIN_RESPEND_MEMPOOLREMOVER_H
#define BITCOIN_RESPEND_MEMPOOLREMOVER_H

#include "respend/respendaction.h"

namespace respend {

// Removes conflicting txes from the mempool
class MempoolRemover : public RespendAction {
    public:

        enum RemoveWhat {
            REMOVE_ALL = 0,
            REMOVE_NON_SI,
        };

        MempoolRemover(RemoveWhat toRemove = REMOVE_ALL,
                       std::list<CTransaction>* removed = nullptr);

        bool AddOutpointConflict(
                const COutPoint&, const CTxMemPool::txiter,
                const CTransaction& respendTx, bool seenBefore,
                bool isEquivalent, bool isSICandidate) override;

        bool IsInteresting() const override;
        void OnValidTrigger(bool v, CTxMemPool&,
                CTxMemPool::setEntries&) override;
        void OnFinishedTrigger() override {}

    private:
        RemoveWhat toRemove;
        std::list<CTransaction>* removed;
        std::list<CTransaction> dummy;
        bool canReplace;
        bool valid;
};

} // ns respend

#endif
