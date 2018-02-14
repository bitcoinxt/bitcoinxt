// Copyright (c) 2017 The Bitcoin XT developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "utilfork.h"
#include "chain.h"
#include "options.h"

// Check if this block activates UAHF. The next block is fork block and must be > 1MB.
bool IsUAHFActivatingBlock(int64_t mtpCurrent, CBlockIndex* pindexPrev) {
    if (!Opt().UAHFTime())
        return false;

    if (mtpCurrent < Opt().UAHFTime())
        return false;

    if (pindexPrev == nullptr)
        // we activated at genesis (happens in regtest)
        return true;

    return pindexPrev->GetMedianTimePast() < Opt().UAHFTime();
}

bool IsMay2018HFActive(int64_t mtpChainTip) {
    int64_t mtpHF = Opt().May2018HFTime();

    if (!mtpHF)
        return false;

    return mtpChainTip >= mtpHF;
}
