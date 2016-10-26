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
