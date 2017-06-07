// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin Core developers
// Copyright (c) 2016 The Bitcoin XT developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef BITCOIN_NODESTATE_H
#define BITCOIN_NODESTATE_H

#include "netbase.h" // CService
#include "uint256.h"
#include "inflightindex.h" // QueuedBlock
#include "sync.h" // CCriticalSection

#include <string>
#include <set>
#include <map>
#include <list>

class CBlockIndex;
class CNode;
class ThinBlockWorker;
class ThinBlockManager;
typedef int NodeId;

struct CBlockReject {
    unsigned char chRejectCode;
    std::string strRejectReason;
    uint256 hashBlock;
};

/**
 * Maintain validation-specific state about nodes, protected by cs_main, instead
 * by CNode's own locks. This simplifies asynchronous operation, where
 * processing of incoming data is done after the ProcessMessage call returns,
 * and we're no longer holding the node's locks.
 */
struct CNodeState {
    //! The peer's address
    CService address;
    //! Whether we have a fully established connection.
    bool fCurrentlyConnected;
    //! Accumulated misbehaviour score for this peer.
    int nMisbehavior;
    //! Whether this peer should be disconnected and banned (unless whitelisted).
    bool fShouldBan;
    //! String name of this peer (debugging/logging purposes).
    std::string name;
    //! List of asynchronously-determined block rejections to notify this peer about.
    std::vector<CBlockReject> rejects;
    //! The best known block we know this peer has announced.
    CBlockIndex *pindexBestKnownBlock;
    //! The hash of the last unknown block this peer has announced.
    uint256 hashLastUnknownBlock;
    //! The last full block we both have.
    CBlockIndex *pindexLastCommonBlock;
    //! The best header we have sent our peer.
    CBlockIndex *bestHeaderSent;
    //! Length of current-streak of unconnecting headers announcements
    int unconnectingHeaders;
    //! Whether we've started headers synchronization with this peer.
    bool fSyncStarted;
    //! Since when we're stalling block download progress (in microseconds), or 0.
    int64_t nStallingSince;
    std::list<QueuedBlock> vBlocksInFlight;
    int nBlocksInFlight;
    int nBlocksInFlightValidHeaders;
    //! Whether we consider this a preferred download peer.
    bool fPreferredDownload;

    //! Whether this peer wants headers (when possible) for block announcements.
    bool prefersHeaders;
    //! Whether this peer wants thin blocks (when possible) for block announcements.
    bool prefersBlocks;

    bool supportsCompactBlocks;

    //! the thin block the node is currently providing to us
    std::shared_ptr<ThinBlockWorker> thinblock;

    CNodeState(NodeId id, ThinBlockManager&);
    ~CNodeState();
};

// Class that maintains per-node state, and
// acts as a RAII smart-pointer that make sure
// the state stays consistent.
class NodeStatePtr {
private:
    static CCriticalSection cs_mapNodeState;
    static std::map<NodeId, CNodeState> mapNodeState;
    CNodeState* s;
    NodeId id;
public:
    static void insert(NodeId nodeid, const CNode *pnode, ThinBlockManager&);

    NodeStatePtr(NodeId nodeid) {
        LOCK(cs_mapNodeState);
        std::map<NodeId, CNodeState>::iterator it = mapNodeState.find(nodeid);
        if (it == mapNodeState.end())
            s = NULL;
        else {
            s = &it->second;
            id = nodeid;
            cs_mapNodeState.lock();
        }
    }
    ~NodeStatePtr() {
        if (s)
            cs_mapNodeState.unlock();
    }
    bool IsNull() const { return s == NULL; }

    CNodeState* operator ->() { return s; }
    const CNodeState* operator ->() const { return s; }

    void erase() {
        if (s) {
            mapNodeState.erase(id);
            s = NULL;
            cs_mapNodeState.unlock();
        }
    }

    static void clear() {
        LOCK(cs_mapNodeState);
        mapNodeState.clear();
    }

private:
    // disallow copy/assignment
    NodeStatePtr(const NodeStatePtr&) {}
    NodeStatePtr& operator=(const NodeStatePtr& p) { return *this; }
};

#endif
