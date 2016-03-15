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

        virtual bool addTx(const CTransaction& tx) { return false; }

        virtual void setAvailable() { }
        virtual bool isAvailable() const { return false; }

        virtual void buildStub(const StubData&, const TxFinder&) { }
        virtual void setToWork(const uint256& block) { }

        virtual void requestBlock(const uint256& block,
                std::vector<CInv>& getDataReq, CNode& node) { }

};

#endif
