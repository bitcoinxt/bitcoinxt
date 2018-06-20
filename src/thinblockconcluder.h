#ifndef BITCOIN_THINBLOCKCONCLUDER_H
#define BITCOIN_THINBLOCKCONCLUDER_H

#include <stdint.h>
#include <vector>

class CNode;
class ThinBlockWorker;
struct XThinReReqResponse;
class CompactReReqResponse;
class BlockInFlightMarker;

// Finishes a block using response from a transaction re-request.
struct XThinBlockConcluder {
    void operator()(const XThinReReqResponse& resp,
                    CConnman&, CNode& pfrom,
                    ThinBlockWorker&, BlockInFlightMarker&);
};

// Finishes a block using response from a transaction re-request.
struct CompactBlockConcluder {
    void operator()(const CompactReReqResponse& resp,
                    CConnman&, CNode& pfrom,
                    ThinBlockWorker&, BlockInFlightMarker&);
};

#endif
