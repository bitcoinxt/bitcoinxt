#include "nodestate.h"
#include "dummythin.h"
#include "net.h" // for CNode
#include "thinblock.h"
#include <utility>

CNodeState::CNodeState(NodeId id, ThinBlockManager& thinblockmg,
                       const CService& addr, const std::string& name)
    : address(addr), name(name)
{
    fCurrentlyConnected = false;
    nMisbehavior = 0;
    fShouldBan = false;
    pindexBestKnownBlock = NULL;
    hashLastUnknownBlock.SetNull();
    pindexLastCommonBlock = NULL;
    bestHeaderSent = nullptr;
    unconnectingHeaders = 0;
    fSyncStarted = false;
    nStallingSince = 0;
    nBlocksInFlight = 0;
    nBlocksInFlightValidHeaders = 0;
    fPreferredDownload = false;
    prefersHeaders = false;
    prefersBlocks = false;
    supportsCompactBlocks = false;
    thinblock.reset(new DummyThinWorker(thinblockmg, id));
}

CCriticalSection NodeStatePtr::cs_mapNodeState;
std::map<NodeId, CNodeState> NodeStatePtr::mapNodeState;

void NodeStatePtr::insert(NodeId nodeid, const CNode *pnode, ThinBlockManager& thinblockmg) {
    LOCK(cs_mapNodeState);
    mapNodeState.insert({nodeid,
                CNodeState(nodeid, thinblockmg, pnode->addr, pnode->addrName)});
}

CNodeState::~CNodeState()
{
}

static bool HasLessOrEqualWork(const CBlockIndex& a, const CBlockIndex& b) {
    return a.nChainWork <= b.nChainWork;
}

bool CNodeState::UpdateBestFromLast(const BlockMap& chainIndex) {

    if (hashLastUnknownBlock.IsNull()) {
        // we have no unknown
        return false;
    }

    auto b = chainIndex.find(hashLastUnknownBlock);
    if (b == end(chainIndex)) {
        // it's still unknown
        return false;
    }
    assert(b->second);

    // we have it, no longer unknown.
    hashLastUnknownBlock.SetNull();
    CBlockIndex* bestCandidate = b->second;
    if (bestCandidate->nChainWork == 0) {
        return false;
    }

    if (pindexBestKnownBlock == nullptr
        || HasLessOrEqualWork(*pindexBestKnownBlock, *bestCandidate))
    {
        pindexBestKnownBlock = bestCandidate;
        return true;
    }

    LogPrint(Log::NET, "%s candidate %s has less work than current best %s (peer %s)\n",
            __func__, pindexBestKnownBlock->GetBlockHash().ToString(),
            bestCandidate->GetBlockHash().ToString(), name);
    return false;
}
