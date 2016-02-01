#ifndef BITCOIN_THINBLOCKCONCLUDER_H
#define BITCOIN_THINBLOCKCONCLUDER_H

#include <stdint.h>
#include <vector>

class CNode;
class ThinBlockWorker;
struct BlockInFlightMarker;
typedef int NodeId;
class CBlockIndex;
class uint256;

namespace Consensus { struct Params; }

// When we receive the "thin block nonce" pong, that marks the end of
// transactions a node is sending us for a thin block.
//
// This marks the end of the transactions we've received. If we get this and
// we have NOT been able to finish reassembling the block, we need to
// re-request the transactions we're missing: this should only happen if we
// download a transaction and then delete it from memory.
struct ThinBlockConcluder {
    public:
        ThinBlockConcluder(BlockInFlightMarker& m) : markInFlight(m)
        {
        }
        virtual ~ThinBlockConcluder() { }
        void operator()(CNode* pfrom,
            uint64_t nonce, ThinBlockWorker& thinblock);

    protected:
        virtual void giveUp(CNode* pfrom, ThinBlockWorker& worker);
        virtual void reRequest(
            CNode* pfrom,
            ThinBlockWorker& worker,
            uint64_t nonce);
        virtual void fallbackDownload(CNode *pfrom, const uint256& block);
        virtual void misbehaving(NodeId, int);

    private:
        BlockInFlightMarker& markInFlight;
};

struct BlockInFlightMarker {
    virtual ~BlockInFlightMarker() = 0;
    virtual void operator()(
        NodeId nodeid, const uint256& hash,
        const Consensus::Params& consensusParams,
        CBlockIndex *pindex) = 0;
};
inline BlockInFlightMarker::~BlockInFlightMarker() { }

#endif
