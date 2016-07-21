#include "nodestate.h"
#include "dummythin.h"
#include "net.h" // for CNode

CNodeState::CNodeState(NodeId id, ThinBlockManager& thinblockmg) {
    fCurrentlyConnected = false;
    nMisbehavior = 0;
    fShouldBan = false;
    pindexBestKnownBlock = NULL;
    hashLastUnknownBlock.SetNull();
    pindexLastCommonBlock = NULL;
    fSyncStarted = false;
    nStallingSince = 0;
    nBlocksInFlight = 0;
    nBlocksInFlightValidHeaders = 0;
    fPreferredDownload = false;
    initialHeadersReceived = false;
    thinblock.reset(new DummyThinWorker(thinblockmg, id));
}

CCriticalSection NodeStatePtr::cs_mapNodeState;
std::map<NodeId, CNodeState> NodeStatePtr::mapNodeState;

void NodeStatePtr::insert(NodeId nodeid, const CNode *pnode, ThinBlockManager& thinblockmg) {
    LOCK(cs_mapNodeState);
    CNodeState &state = mapNodeState.insert(std::make_pair(
                nodeid, CNodeState(nodeid, thinblockmg))).first->second;
    state.name = pnode->addrName;
    state.address = pnode->addr;
}
