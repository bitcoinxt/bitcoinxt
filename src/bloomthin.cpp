#include "bloomthin.h"
#include "protocol.h"
#include <utility>
#include <sstream>


BloomThinWorker::BloomThinWorker(ThinBlockManager& m, NodeId n) :
    ThinBlockWorker(m, n)
{
}

void BloomThinWorker::requestBlock(const uint256& block,
        std::vector<CInv>& getDataReq, CNode& node) {
    getDataReq.push_back(CInv(MSG_FILTERED_BLOCK, block));
}

CBlockHeader ThinBloomStub::header() const {
    return merkleblock.header;
}

std::vector<ThinTx> ThinBloomStub::allTransactions() const {
    std::vector<uint256> txHashes;

    // Has a side effect of validating the MerkeleBlock hash
    // and can throw a thinblock_error
    uint256 merkleRoot = CMerkleBlock(merkleblock).txn.ExtractMatches(txHashes);
    if (merkleblock.header.hashMerkleRoot != merkleRoot)
        throw thinblock_error("Failed to match Merkle root or bad tree in thin block");

    return std::vector<ThinTx>(txHashes.begin(), txHashes.end());
}

std::vector<CTransaction> ThinBloomStub::missingProvided() const {
    return std::vector<CTransaction>();
}
