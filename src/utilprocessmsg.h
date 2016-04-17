#ifndef BITCOIN_UTILPROCESSMSG_H
#define BITCOIN_UTILPROCESSMSG_H

#include <vector>
class uint256;
class CNode;
class CBlockHeader;

bool HaveBlockData(const uint256& hash);

// Process received block header.
struct BlockHeaderProcessor {

    // returns false on error
    virtual bool operator()(const std::vector<CBlockHeader>& headers, bool peerSentMax) = 0;

    virtual ~BlockHeaderProcessor() = 0;
};
inline BlockHeaderProcessor::~BlockHeaderProcessor() { }

#endif
