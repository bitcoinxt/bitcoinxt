// Copyright (c) 2017 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "consensus/validation.h"
#include "main.h"

bool CheckFinalTx(const CTransaction &tx, int flags) {
    CValidationState state;
    return ContextualCheckTransactionForNextBlock(tx, state, flags);
}
