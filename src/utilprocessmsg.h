#ifndef BITCOIN_UTILPROCESSMSG_H
#define BITCOIN_UTILPROCESSMSG_H

#include <vector>
#include <cstdint>

class uint256;
class CNode;
class CBlockHeader;
class CBlockIndex;
namespace Consensus { struct Params; }

typedef int NodeId;

bool HaveBlockData(const uint256& hash);

class BlockInFlightMarker {
public:
    virtual ~BlockInFlightMarker() = 0;
    virtual void operator()(
        NodeId nodeid, const uint256& hash,
        const Consensus::Params& consensusParams,
        const CBlockIndex* pindex) = 0;
};
inline BlockInFlightMarker::~BlockInFlightMarker() { }

// If node matches our criteria for outgoing connections.
bool KeepOutgoingPeer(const CNode&);

void UpdateBestHeaderSent(CNode& node, CBlockIndex* blockIndex);

// Exponentially limit the rate of nSize flow to nLimit.  nLimit unit is thousands-per-minute.
bool RateLimitExceeded(double& dCount, int64_t& nLastTime, int64_t nLimit, unsigned int nSize);

#endif
