#ifndef BITCOIN_THINBLOCKUTIL_H
#define BITCOIN_THINBLOCKUTIL_H

#include "net.h"
#include "thinblockmanager.h"
#include "thinblock.h"

class CBlock;

// Utils for thin block related unit tests
CBlock TestBlock1();
CBlock TestBlock2();

struct NullFinder : public TxFinder {
    virtual CTransaction operator()(const ThinTx& hash) const {
        return CTransaction();
    }
};

struct DummyNode : public CNode {
    DummyNode() : CNode(INVALID_SOCKET, CAddress()) {
        id = 42;
        nVersion = PROTOCOL_VERSION;
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


#endif
