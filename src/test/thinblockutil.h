#ifndef BITCOIN_THINBLOCKUTIL_H
#define BITCOIN_THINBLOCKUTIL_H

#include "net.h"
#include "thinblock.h"
#include "nodestate.h"
#include "thinblockmanager.h"
#include "protocol.h"
#include "utilprocessmsg.h"
#include <memory>

class CBlock;

// Utils for thin block related unit tests
CBlock TestBlock1();
CBlock TestBlock2();
std::unique_ptr<ThinBlockManager> GetDummyThinBlockMg();

struct NullFinder : public TxFinder {
    virtual CTransaction operator()(const ThinTx& hash) const {
        return CTransaction();
    }
};

struct DummyNode : public CNode {
        DummyNode(NodeId myid = 42, ThinBlockManager* mgr = nullptr) : CNode(myid, NODE_NETWORK, INVALID_SOCKET, CAddress()) {
        static auto staticmgr = GetDummyThinBlockMg();
        if (!mgr)
            mgr = staticmgr.get();

        NodeStatePtr::insert(id, this, *mgr);
        nVersion = PROTOCOL_VERSION;
    }
    virtual ~DummyNode() {
        bool fUpdateConnectionTime;
        GetNodeSignals().FinalizeNode(id, fUpdateConnectionTime);
        NodeStatePtr(id).erase();
    }
    virtual void BeginMessage(const char* pszCommand) EXCLUSIVE_LOCK_FUNCTION(cs_vSend) {
        messages.push_back(pszCommand);
        CNode::BeginMessage(pszCommand);
    }

    std::vector<std::string> messages;
};

struct DummyFinishedCallb : public ThinBlockFinishedCallb {
    virtual void operator()(const CBlock&, const std::vector<NodeId>&) { }
};

struct DummyInFlightEraser : public InFlightEraser {
    virtual void operator()(NodeId, const uint256& hash) { }
};

struct DummyMarkAsInFlight : public BlockInFlightMarker {

    virtual void operator()(
        NodeId nodeid, const uint256& hash,
        const Consensus::Params& consensusParams,
        CBlockIndex *pindex)
    {
        block = hash;
    }
    uint256 block;
};


#endif
