// Copyright (c) 2017 The Bitcoin XT developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "utilfork.h"
#include "chain.h"
#include "options.h"

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

    if (!mtpHF)
        return false;

    return mtpChainTip >= mtpHF;
}
