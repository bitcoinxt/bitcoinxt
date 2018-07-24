// Copyright (c) 2018 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "respend/respendlogger.h"
#include "util.h"

namespace respend {

RespendLogger::RespendLogger() :
    equivalent(false), valid("indeterminate"), newConflict(false)
{
}

bool RespendLogger::AddOutpointConflict(
        const COutPoint&, const CTxMemPool::txiter mempoolEntry,
        const CTransaction& respendTx, bool seenBefore,
        bool isEquivalent, bool isSICandidate)
{
    orig = mempoolEntry->GetTx().GetHash().ToString();
    respend = respendTx.GetHash().ToString();
    equivalent = isEquivalent;
    newConflict = newConflict || !seenBefore;

    // We have enough info for logging purposes.
    return false;
}

bool RespendLogger::IsInteresting() const {
    // Logging never triggers full tx validation
    return false;
}

void RespendLogger::OnFinishedTrigger() {
    if (respend.empty() || !newConflict)
        return;

    const std::string msg = "respend: Tx %s conflicts with %s"
        " (new conflict: %s, equivalent %s, valid %s)\n";

    LogPrint(Log::RESPEND, msg.c_str(), orig, respend,
              newConflict ? "yes" : "no",
              equivalent ? "yes" : "no", valid);
}

}
