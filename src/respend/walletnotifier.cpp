// Copyright (c) 2018 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "respend/walletnotifier.h"
#include "validationinterface.h"

namespace respend {

WalletNotifier::WalletNotifier() : valid(false), interesting(false)
{
}

bool WalletNotifier::AddOutpointConflict(
        const COutPoint&, const CTxMemPool::txiter,
        const CTransaction& respendTx, bool seenBefore,
        bool isEquivalent, bool isSICandidate)
{
    if (isEquivalent || seenBefore)
        return true; // look at more outpoints

    if (interesting)
        return false; // we already have a copy

    interesting = true;
    this->respendTx = respendTx;

    // we have enough info
    return false;
}

bool WalletNotifier::IsInteresting() const {
    return interesting;
}

void WalletNotifier::SetValid(bool v) {
    valid = v;
}

void WalletNotifier::Trigger() {
    if (!valid || !interesting) {
        return;
    }
    SyncWithWallets(respendTx, nullptr, true);
}

} // ns respend
