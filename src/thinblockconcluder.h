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
struct XThinReReqResponse;
class CompactReReqResponse;

namespace Consensus { struct Params; }

// When we receive the "thin block nonce" pong, that marks the end of
// transactions a node is sending us for a thin block.
//
// This marks the end of the transactions we've received. If we get this and
// we have NOT been able to finish reassembling the block, we need to
// re-request the transactions we're missing: this should only happen if we
// download a transaction and then delete it from memory.
struct BloomBlockConcluder {
    public:
        BloomBlockConcluder(BlockInFlightMarker& m) : markInFlight(m)
        {
        }
        virtual ~BloomBlockConcluder() { }
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

// Finishes a block using response from a transaction re-request.
struct XThinBlockConcluder {
    void operator()(const XThinReReqResponse& resp,
        CNode& pfrom, ThinBlockWorker& worker);
};

// Finishes a block using response from a transaction re-request.
struct CompactBlockConcluder {
    void operator()(const CompactReReqResponse& resp,
        CNode& pfrom, ThinBlockWorker& worker);
};

#endif
