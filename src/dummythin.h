// Copyright (c) 2015-2016 The Bitcoin XT developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef BITCOIN_DUMMYTHIN_H
#define BITCOIN_DUMMYTHIN_H

#include "thinblock.h"
#include "protocol.h" // for CInv

class DummyThinWorker : public ThinBlockWorker {

    public:
        DummyThinWorker(ThinBlockManager& mg, NodeId id)
            : ThinBlockWorker(mg, id) { }

        bool addTx(const uint256&, const CTransaction& tx) override { return false; }

        void buildStub(const StubData&, const TxFinder&) override { }
        void addWork(const uint256& block) override { }
        void stopWork(const uint256& block) override { }
        void stopAllWork() override { }

        void requestBlock(const uint256& block,
                std::vector<CInv>& getDataReq, CNode& node) override { }
};

#endif
