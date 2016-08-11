#include "utilprocessmsg.h"
#include "main.h" // mapBlockIndex
#include "options.h"

bool HaveBlockData(const uint256& hash) {
    return mapBlockIndex.count(hash)
        && mapBlockIndex.find(hash)->second->nStatus & BLOCK_HAVE_DATA;
}

bool SupportsThinBlocks(const CNode& node) {
    return node.SupportsBloomThinBlocks() || node.SupportsXThinBlocks()
        || node.SupportsCompactBlocks();
}

bool KeepOutgoingPeer(const CNode& node) {
    assert(!node.fInbound);

    if (!Opt().UsingThinBlocks())
        return true;

    if (Opt().OptimalThinBlocksOnly())
        return node.SupportsXThinBlocks() || node.SupportsCompactBlocks();

    return SupportsThinBlocks(node);
}
