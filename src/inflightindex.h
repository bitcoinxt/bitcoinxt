// Copyright (c) 2016 The Bitcoin XT developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef BITCOIN_INFLIGHTINDEX_H
#define BITCOIN_INFLIGHTINDEX_H

#include "uint256.h"
#include <list>
#include <set>

typedef int NodeId;
class CBlockIndex;

/** Blocks that are in flight, and that are in the queue to be downloaded. Protected by cs_main. */
struct QueuedBlock {
    uint256 hash;
    CBlockIndex *pindex;  //! Optional.
    int64_t nTime;  //! Time of "getdata" request in microseconds.
    bool fValidatedHeaders;  //! Whether this block has validated headers at the time of request.
    int64_t nTimeDisconnect; //! The timeout for this block request (for disconnecting a slow peer)
    NodeId node;
};

typedef std::list<QueuedBlock>::iterator QueuedBlockPtr;

// Keeps track of blocks in flight.
struct InFlightIndex {
    virtual ~InFlightIndex() { }
    void erase(const QueuedBlockPtr& ptr);
    void erase(NodeId nodeid, const uint256& block);
    void insert(const QueuedBlockPtr& queued);
    virtual bool isInFlight(const uint256& block) const;
    std::set<NodeId> nodesWithQueued(const uint256& block) const;
    std::vector<QueuedBlockPtr> queuedPtrsFor(const uint256& block) const;
    QueuedBlockPtr queuedItem(NodeId, const uint256& block) const;
    void clear();

    private:
    std::vector<QueuedBlockPtr> inFlight;
};

#endif
