#include "inflightindex.h"
#include "util.h"

void InFlightIndex::erase(const QueuedBlockPtr& ptr) {
    std::vector<QueuedBlockPtr>::iterator pos
        = std::find(inFlight.begin(), inFlight.end(), ptr);

    if (pos == inFlight.end()) {
        LogPrintf("Warn: queued block ptr not found, can't erase");
        return;
    }

    inFlight.erase(pos);
}

void InFlightIndex::erase(NodeId nodeid, const uint256& block) {
    typedef std::vector<QueuedBlockPtr>::iterator auto_;
    for (auto_ b = inFlight.begin(); b != inFlight.end(); ++b) {
        if ((*b)->node == nodeid && (*b)->hash == block) {
            inFlight.erase(b);
            return;
        }
    }
}

void InFlightIndex::insert(const QueuedBlockPtr& queued) {
    inFlight.push_back(queued);
}

bool InFlightIndex::isInFlight(const uint256& block) const {
    typedef std::vector<QueuedBlockPtr>::const_iterator auto_;
    for (auto_ b = inFlight.begin(); b != inFlight.end(); ++b)
        if ((*b)->hash == block)
            return true;
    return false;
}


std::set<NodeId> InFlightIndex::nodesWithQueued(const uint256& block) const {
    std::set<NodeId> ids;

    typedef std::vector<QueuedBlockPtr>::const_iterator auto_;
    std::vector<QueuedBlockPtr> queued = queuedPtrsFor(block);
    for (auto_ b = queued.begin(); b != queued.end(); ++b)
        ids.insert((*b)->node);

    return ids;
}

std::vector<QueuedBlockPtr> InFlightIndex::queuedPtrsFor(const uint256& block) const {
    std::vector<QueuedBlockPtr> q;

    typedef std::vector<QueuedBlockPtr>::const_iterator auto_;
    for (auto_ b = inFlight.begin(); b != inFlight.end(); ++b) {
        if ((*b)->hash != block)
            continue;
        q.push_back(*b);
    }
    return q;
}

void InFlightIndex::clear() {
    inFlight.clear();
}
