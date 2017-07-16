#include "utilprocessmsg.h"
#include "chain.h"
#include "main.h" // mapBlockIndex
#include "options.h"
#include "net.h"
#include "nodestate.h"

bool HaveBlockData(const uint256& hash) {
    return mapBlockIndex.count(hash)
        && mapBlockIndex.find(hash)->second->nStatus & BLOCK_HAVE_DATA;
}

bool SupportsThinBlocks(const CNode& node) {
    return node.SupportsXThinBlocks() || node.SupportsCompactBlocks();
}

bool KeepOutgoingPeer(const CNode& node) {
    assert(!node.fInbound);

    // UASF nodes reject blocks that are valid, but do not vote for a
    // consensus change the node operator is trying to force onto the network.
    if (node.strSubVer.find("UASF") != std::string::npos)
        return false;

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
