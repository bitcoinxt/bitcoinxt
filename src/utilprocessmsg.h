#ifndef BITCOIN_UTILPROCESSMSG_H
#define BITCOIN_UTILPROCESSMSG_H

#include <vector>
class uint256;
class CNode;
class CBlockHeader;
class CBlockIndex;
namespace Consensus { struct Params; }

typedef int NodeId;

bool HaveBlockData(const uint256& hash);

struct BlockInFlightMarker {
    virtual ~BlockInFlightMarker() = 0;
    virtual void operator()(
        NodeId nodeid, const uint256& hash,
        const Consensus::Params& consensusParams,
        CBlockIndex* pindex) = 0;
};
inline BlockInFlightMarker::~BlockInFlightMarker() { }

#endif
