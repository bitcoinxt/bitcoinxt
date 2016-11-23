// Copyright (c) 2016- The Bitcoin XT developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef BITCOIN_COMPACTTHIN_H
#define BITCOIN_COMPACTTHIN_H

#include "blockencodings.h"
#include "thinblock.h"
#include "util.h"

// Core also wanted to make their own thin
// blocks solution; Compact Blocks (BIP152)
//
// This is our implementation of it.

class CompactWorker : public ThinBlockWorker {

    public:
        CompactWorker(ThinBlockManager&, NodeId);

        void requestBlock(const uint256& block,
                std::vector<CInv>& getDataReq, CNode& node) override;
};


struct CompactStub : public StubData {
    CompactStub(const CompactBlock& b) : block(b) {
        LogPrint("thin", "Created compact stub for %s, %d transactions.\n",
                header().GetHash().ToString(), allTransactions().size());

    }

    CBlockHeader header() const override {
        return block.header;
    }

    std::vector<ThinTx> allTransactions() const override;

    // Transactions provided in the stub
    std::vector<CTransaction> missingProvided() const override {
        std::vector<CTransaction> txs;

        for (auto& p : block.prefilledtxn)
            txs.push_back(p.tx);

        return txs;
    }

    private:
        CompactBlock block;
};

#endif
