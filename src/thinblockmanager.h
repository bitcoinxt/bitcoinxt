// Copyright (c) 2015-2016 The Bitcoin XT developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef BITCOIN_THINBLOCKMG
#define BITCOIN_THINBLOCKMG

#include "uint256.h"
#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>
#include <memory>
#include <set>
#include <map>
#include <vector>

class CBlock;
class TxFinder;
struct ThinBloomStub;
struct XThinStub;
struct StubData;
class ThinBlockBuilder;
class ThinBlockWorker;
class CTransaction;
class ThinTx;
typedef int NodeId;

// Call when a block is reassembled.
struct ThinBlockFinishedCallb {
    virtual void operator()(const CBlock&,
            const std::vector<NodeId>& contributors) = 0;
    virtual ~ThinBlockFinishedCallb() = 0;
};
inline ThinBlockFinishedCallb::~ThinBlockFinishedCallb() { }

// Erase a nodes inflight block queue item.
struct InFlightEraser {
    virtual void operator()(NodeId, const uint256& block) = 0;
    virtual ~InFlightEraser() = 0;
};
inline InFlightEraser::~InFlightEraser() { }

// Keeps state of active thin block downloads.
class ThinBlockManager : boost::noncopyable {
    public:
        ThinBlockManager(
                std::unique_ptr<ThinBlockFinishedCallb> callb,
                std::unique_ptr<InFlightEraser> inFlightEraser);

        void addWorker(const uint256& block, ThinBlockWorker& w);
        void delWorker(ThinBlockWorker& w, NodeId);
        int numWorkers(const uint256& block) const;

        void buildStub(const StubData&, const TxFinder& txFinder);
        bool isStubBuilt(const uint256& block);

        bool addTx(const uint256& block, const CTransaction& tx);
        void removeIfExists(const uint256& block);
        std::vector<ThinTx> getTxsMissing(const uint256& block) const;

    private:
        struct ActiveBuilder {
            boost::shared_ptr<ThinBlockBuilder> builder;
            std::set<ThinBlockWorker*> workers;
        };
        std::map<uint256, ActiveBuilder> builders;
        std::unique_ptr<ThinBlockFinishedCallb> finishedCallb;
        std::unique_ptr<InFlightEraser> inFlightEraser;

        void finishBlock(const uint256& block, ThinBlockBuilder&);
};

#endif
