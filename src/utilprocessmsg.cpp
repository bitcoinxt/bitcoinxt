#include "utilprocessmsg.h"
#include "chain.h"
#include "main.h" // mapBlockIndex
#include "net.h"
#include "netmessagemaker.h"
#include "nodestate.h"
#include "options.h"
#include "utiltime.h"

bool HaveBlockData(const uint256& hash) {
    return mapBlockIndex.count(hash)
        && mapBlockIndex.find(hash)->second->nStatus & BLOCK_HAVE_DATA;
}

static bool SupportsThinBlocks(const CNode& node) {
    return node.SupportsXThinBlocks() || node.SupportsCompactBlocks();
}

bool KeepOutgoingPeer(const CNode& node) {
    assert(!node.fInbound);

    if (!Opt().UsingThinBlocks())
        return true;

    return SupportsThinBlocks(node);
}

void UpdateBestHeaderSent(CNode& node, CBlockIndex* blockIndex) {
    // When we send a block, were also sending its header.
    NodeStatePtr state(node.id);
    if (!state->bestHeaderSent)
        state->bestHeaderSent = blockIndex;

    if (state->bestHeaderSent->nHeight <= blockIndex->nHeight)
        state->bestHeaderSent = blockIndex;
}

// Exponentially limit the rate of nSize flow to nLimit.  nLimit unit is thousands-per-minute.
bool RateLimitExceeded(double& dCount, int64_t& nLastTime, int64_t nLimit, unsigned int nSize)
{
    int64_t nNow = GetTime();
    dCount *= pow(1.0 - 1.0/600.0, (double)(nNow - nLastTime));
    nLastTime = nNow;
    if (dCount >= nLimit*10*1000)
        return true;
    dCount += nSize;
    return false;
}

bool ProcessRejectsAndBans(CConnman* connman, CNode* pnode) {
    NodeStatePtr statePtr(pnode->GetId());
    if (connman == nullptr || pnode == nullptr || statePtr.IsNull()) {
        LogPrint(Log::NET, "%s got invalid argments\n", __func__);
        return true;
    }

    for (const CBlockReject& reject : statePtr->rejects) {
        connman->PushMessage(pnode, CNetMsgMaker(INIT_PROTO_VERSION).Make(
                    NetMsgType::REJECT, std::string(NetMsgType::BLOCK),
                    reject.chRejectCode,
                    reject.strRejectReason, reject.hashBlock));
    }
    statePtr->rejects.clear();

    if (!statePtr->fShouldBan)
        return false;

    statePtr->fShouldBan = false;
    if (pnode->fWhitelisted) {
        LogPrintf("Warning: not punishing whitelisted peer %s!\n", pnode->addr.ToString());
        return false;
    }
    pnode->fDisconnect = true;
    if (pnode->addr.IsLocal()) {
        LogPrintf("Warning: not banning local peer %s!\n", pnode->addr.ToString());
    }
    else {
        LogPrint(Log::NET, "Banning node %d (%s)\n", pnode->GetId(), pnode->addr.ToString());
        connman->Ban(pnode->addr);
    }
    return true;
}
