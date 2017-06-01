#ifndef BITCOIN_THINBLOCKCONCLUDER_H
#define BITCOIN_THINBLOCKCONCLUDER_H

#include <stdint.h>
#include <vector>

class CNode;
class ThinBlockWorker;
struct XThinReReqResponse;
class CompactReReqResponse;

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
