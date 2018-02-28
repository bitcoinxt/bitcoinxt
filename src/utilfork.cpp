// Copyright (c) 2017 The Bitcoin XT developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "utilfork.h"
#include "chain.h"
#include "options.h"

static bool IsForkActivatingBlock(int64_t mtpActivationTime,
                                  int64_t mtpCurrent, CBlockIndex* pindexPrev)
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
bool IsUAHFActivatingBlock(int64_t mtpCurrent, CBlockIndex* pindexPrev) {
    return IsForkActivatingBlock(Opt().UAHFTime(), mtpCurrent, pindexPrev);
}

bool IsThirdHFActivatingBlock(int64_t mtpCurrent, CBlockIndex* pindexPrev) {
    return IsForkActivatingBlock(Opt().ThirdHFTime(), mtpCurrent, pindexPrev);
}

bool IsThirdHFActive(int64_t mtpChainTip) {
    int64_t mtpHF = Opt().ThirdHFTime();

    if (!mtpHF)
        return false;

    return mtpChainTip >= mtpHF;
}
