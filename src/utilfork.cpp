// Copyright (c) 2017 - 2018 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "utilfork.h"
#include "chain.h"
#include "options.h"
#include "txmempool.h"
#include "util.h"

static bool IsForkActivatingBlock(int64_t mtpActivationTime,
                                  int64_t mtpCurrent, const CBlockIndex* pindexPrev)
{
    if (!mtpActivationTime)
        return false;

    if (mtpCurrent < mtpActivationTime)
        return false;

    if (pindexPrev == nullptr)
        // we activated at genesis (happens in regtest)
        return true;

    return pindexPrev->GetMedianTimePast() < mtpActivationTime;
}

// Check if this block activates UAHF. The next block is fork block and must be > 1MB.
bool IsUAHFActivatingBlock(int64_t mtpCurrent, const CBlockIndex* pindexPrev) {
    return IsForkActivatingBlock(Opt().UAHFTime(), mtpCurrent, pindexPrev);
}

bool IsThirdHFActivatingBlock(int64_t mtpCurrent, const CBlockIndex* pindexPrev) {
    return IsForkActivatingBlock(Opt().ThirdHFTime(), mtpCurrent, pindexPrev);
}

bool IsFourthHFActivatingBlock(int64_t mtpCurrent, const CBlockIndex* pindexPrev) {
    return IsForkActivatingBlock(Opt().FourthHFTime(), mtpCurrent, pindexPrev);
}

static bool IsForkActive(uint64_t mtpActivation, uint64_t mtpChainTip) {
    if (!mtpActivation)
        return false;

    return mtpChainTip >= mtpActivation;
}

bool IsUAHFActive(uint64_t mtpChainTip) {
    return IsForkActive(Opt().UAHFTime(), mtpChainTip);
}

bool IsThirdHFActive(int64_t mtpChainTip) {
    return IsForkActive(Opt().ThirdHFTime(), mtpChainTip);
}

bool IsFourthHFActive(int64_t mtpChainTip) {
    return IsForkActive(Opt().FourthHFTime(), mtpChainTip);
}

void ForkMempoolClearer(
        CTxMemPool& mempool, const CBlockIndex* oldTip, const CBlockIndex* newTip)
{
    if (oldTip == nullptr || newTip == nullptr || oldTip == newTip)
        return;

    const bool rollback = oldTip->nHeight > newTip->nHeight;
    const uint64_t mtpOld = oldTip->GetMedianTimePast();
    const uint64_t mtpNew = newTip->GetMedianTimePast();

    if (rollback) {
        // Tip is being rollbacked. This is caused by reorg or invalidateblock
        // call. Check if fork with incompatible transactions is deactivated.
        if (oldTip->pprev == nullptr)
            return;

        const uint64_t mtpOldPrev = oldTip->pprev->GetMedianTimePast();

        bool forkUndone =
            (IsUAHFActive(mtpOld) && !IsUAHFActive(mtpOldPrev))
            || (IsThirdHFActive(mtpOld) && !IsThirdHFActive(mtpOldPrev));

        if (forkUndone) {
            LogPrint(Log::BLOCK, "Rollback past fork - clearing mempool.\n");
            mempool.clear();
        }
        return;
    }

    // Block appended to chain.
    // Check if a fork with incompatible transactions is activated.
    if (IsUAHFActivatingBlock(mtpNew, oldTip))
    {
            LogPrint(Log::BLOCK, "HF activating block - clearing mempool.\n");
            mempool.clear();
    }
}
