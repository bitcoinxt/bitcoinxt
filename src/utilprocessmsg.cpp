#include "utilprocessmsg.h"
#include "chain.h"
#include "main.h" // mapBlockIndex
#include "net.h"
#include "nodestate.h"
#include "options.h"
#include "utiltime.h"

bool HaveBlockData(const uint256& hash) {
    return mapBlockIndex.count(hash)
        && mapBlockIndex.find(hash)->second->nStatus & BLOCK_HAVE_DATA;
}

namespace {

bool SupportsThinBlocks(const CNode& node) {
    return node.SupportsXThinBlocks() || node.SupportsCompactBlocks();
}

bool SignalsUAHF(const CNode& node) {
    return node.nServices & NODE_BITCOIN_CASH;
}

} // ns anon

bool KeepOutgoingPeer(const CNode& node) {
    assert(!node.fInbound);

    // Thin blocks enabled. We want thin block peers only.
    if (Opt().UsingThinBlocks() && !SupportsThinBlocks(node))
        return false;

    // When we're on the UAHF fork, we want UAHF peers only.
    // When we're not, we don't want UAHF peers.
    return Opt().UAHFTime()
        ? SignalsUAHF(node)
        : !SignalsUAHF(node);
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
