// Copyright (c) 2015-2016 The Bitcoin XT developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef BITCOIN_BLOOMTHIN_H
#define BITCOIN_BLOOMTHIN_H

#include "merkleblock.h"
#include "thinblock.h"

class CTransaction;
class ThinBlockManager;
typedef int NodeId;

struct ThinBloomStub : public StubData {
    ThinBloomStub(const CMerkleBlock& m) : merkleblock(m) { }

    virtual CBlockHeader header() const;
    virtual std::vector<ThinTx> allTransactions() const;
    virtual std::vector<CTransaction> missingProvided() const;

    private:
        CMerkleBlock merkleblock;
};

// Thin blocks from peer that support bloom filters.
class BloomThinWorker : public ThinBlockWorker {
    public:
        BloomThinWorker(ThinBlockManager& m, NodeId);

        virtual void requestBlock(const uint256& block,
                std::vector<CInv>& getDataReq, CNode& node);
};


#endif
