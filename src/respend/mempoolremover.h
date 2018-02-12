// Copyright (c) 2018 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef BITCOIN_RESPEND_MEMPOOLREMOVER_H
#define BITCOIN_RESPEND_MEMPOOLREMOVER_H

#include "respend/respendaction.h"

namespace respend {

// Removes any conflicting txes from the mempool
class MempoolRemover : public RespendAction {
    public:
        MempoolRemover(CTxMemPool& pool, std::list<CTransaction>& removed);

        bool AddOutpointConflict(
                const COutPoint&, const CTxMemPool::txiter,
                const CTransaction& respendTx,
                bool seenBefore, bool isEquivalent) override;

        bool IsInteresting() const override;
        void SetValid(bool v) override {
            valid = v;
        }
        void Trigger() override;

    private:
        CTxMemPool& pool;
        std::list<CTransaction>& removed;
        CTxMemPool::setEntries tx1s;
        bool valid;
};

} // ns respend

#endif
