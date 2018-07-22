// Copyright (c) 2018 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "respend/respenddetector.h"
#include "bloom.h"
#include "options.h"
#include "respend/respendaction.h"
#include "respend/respendlogger.h"
#include "respend/respendrelayer.h"
#include "respend/walletnotifier.h"
#include "util.h"

#include <algorithm>

namespace respend {

static const unsigned int MAX_RESPEND_BLOOM = 100000;
std::unique_ptr<CRollingBloomFilter> RespendDetector::respentBefore;
std::mutex RespendDetector::respentBeforeMutex;

std::vector<RespendActionPtr> CreateDefaultActions(CConnman* connman) {
    std::vector<RespendActionPtr> actions;

    if (connman) {
        actions.push_back(RespendActionPtr(new RespendRelayer{connman}));
    }
    if (LogAcceptCategory(Log::RESPEND)) {
        actions.push_back(RespendActionPtr(new RespendLogger{}));
    }
#ifdef ENABLE_WALLET
    actions.push_back(RespendActionPtr(new WalletNotifier{}));
#endif
    return actions;
}

RespendDetector::RespendDetector(
        const CTxMemPool& pool, const CTransaction& tx,
        std::vector<RespendActionPtr> actions) : actions(actions)
{
    {
        std::lock_guard<std::mutex> lock(respentBeforeMutex);
        if (!bool(respentBefore)) {
            respentBefore.reset(new CRollingBloomFilter(MAX_RESPEND_BLOOM, 0.01));
        }
    }
    CheckForRespend(pool, tx);
}

RespendDetector::~RespendDetector() {
    // Time for actions to perform their task using the (limited)
    // information they've gathered.
    for (auto& a : actions) {
        try {
            a->Trigger();
        }
        catch (const std::exception& e) {
            LogPrintf("respend: ERROR - respend action threw: %s\n", e.what());
        }
   }
}

void RespendDetector::CheckForRespend(
        const CTxMemPool& pool, const CTransaction& tx) {

    LOCK(pool.cs); // protect pool.mapNextTx

    for (const CTxIn& in : tx.vin)
    {
        const COutPoint outpoint = in.prevout;

        // Is there a conflicting spend?
        auto spendIter = pool.mapNextTx.find(outpoint);
        if (spendIter == pool.mapNextTx.end())
            continue;

        CTxMemPool::txiter poolIter = pool.mapTx.find(spendIter->second.ptx->GetHash());
        if (poolIter == pool.mapTx.end() || poolIter->GetTx() == tx)
            continue;

        conflictingEntries.insert(poolIter);

        bool collectMore = false;
        bool seenBefore;
        {
            std::lock_guard<std::mutex> lock(respentBeforeMutex);
            seenBefore = respentBefore->contains(poolIter->GetTx().GetHash());
        }
        for (auto& a : actions) {
            // Actions can return true if they want to check more
            // outpoints for conflicts.
            bool m = a->AddOutpointConflict(outpoint, poolIter, tx, seenBefore,
                    tx.IsEquivalentTo(poolIter->GetTx()), pool.HasNoInputsOf(tx));
            collectMore = collectMore || m;
        }
        if (!collectMore)
            return;
    }
}

void RespendDetector::SetValid(bool valid) {
    if (valid) {
        std::lock_guard<std::mutex> lock(respentBeforeMutex);
        for (auto& it : conflictingEntries) {
            respentBefore->insert(it->GetTx().GetHash());
        }
    }
    for (auto& a : actions) {
        a->SetValid(valid);
    }
}

bool RespendDetector::IsRespend() const {
    return !conflictingEntries.empty();
}

bool RespendDetector::IsInteresting() const {
    return std::any_of(begin(actions), end(actions), [](const RespendActionPtr& a) {
            return a->IsInteresting();
        });
}

} // ns respend
