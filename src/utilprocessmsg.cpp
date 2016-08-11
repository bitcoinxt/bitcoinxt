#include "utilprocessmsg.h"
#include "main.h" // mapBlockIndex
#include "options.h"

bool HaveBlockData(const uint256& hash) {
    return mapBlockIndex.count(hash)
        && mapBlockIndex.find(hash)->second->nStatus & BLOCK_HAVE_DATA;
}

bool SupportsThinBlocks(const CNode& node) {
    return node.SupportsBloomThinBlocks() || node.SupportsXThinBlocks();
}

bool KeepOutgoingPeer(const CNode& node) {
    assert(!node.fInbound);

    if (!Opt().UsePeerSelection())
        return true;
    
    if (Opt().UsingThinBlocks() && !SupportsThinBlocks(node))
        return false;

    if (Opt().XThinBlocksOnly() && !node.SupportsXThinBlocks())
        return false;

    return true;
}
