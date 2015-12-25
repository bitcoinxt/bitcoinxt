// Copyright (c) 2015 The Bitcoin XT developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef BITCOIN_THINBLOCKBUILDER_H
#define BITCOIN_THINBLOCKBUILDER_H

#include "uint256.h"
#include <stdexcept>
#include <vector>
#include "primitives/block.h"
#include <memory>
#include <boost/noncopyable.hpp>

class CMerkleBlock;
class CTransaction;
class CBlock;
typedef int NodeId;

struct TxFinder {
    // returned tx is null if not found (and needs to be downloaded)
    virtual CTransaction operator()(const uint256& hash) const = 0;
    virtual ~TxFinder() = 0;
};
inline TxFinder::~TxFinder() { }

const size_t NOT_BUILDING = -1;

class ThinBlockBuilder {
    public:
        ThinBlockBuilder(const CMerkleBlock& m, const TxFinder& txFinder);
        ThinBlockBuilder() : missing(NOT_BUILDING) { }

        bool isValid() const;

        enum TXAddRes {
            TX_ADDED,
            TX_UNWANTED,
            TX_DUP
        };

        TXAddRes addTransaction(const CTransaction& tx);

        int numTxsMissing() const;
        std::vector<uint256> getTxsMissing() const;

        // Tries to build the block. Throws thinblock_error if it fails.
        // Returns the block (and invalidates this object)
        CBlock finishBlock();

        void reset();

    private:
        CBlock thinBlock;
        std::vector<uint256> wantedTxs;
        size_t missing;

        // Has a side effect of validating the MerkeleBlock hash
        // and can throw a thinblock_error
        std::vector<uint256> getHashes(const CMerkleBlock& m) const;
};

struct ThinBlockFinishedCallb {
    virtual void operator()(const CBlock&, const std::vector<NodeId>&) = 0;
    virtual ~ThinBlockFinishedCallb() = 0;
};
inline ThinBlockFinishedCallb::~ThinBlockFinishedCallb() { }

class ThinBlockWorker;

// Keeps state of active thin block downloads.
class ThinBlockManager : boost::noncopyable {
    public:

        ThinBlockManager(std::auto_ptr<ThinBlockFinishedCallb> callb);
        void addWorker(const uint256& block, ThinBlockWorker& w);
        void delWorker(ThinBlockWorker& w);
        int numWorkers(const uint256& block) const;

        void buildStub(const CMerkleBlock& m, const TxFinder& txFinder);
        bool isStubBuilt(const uint256& block) const;

        bool addTx(const uint256& block, const CTransaction& tx);
        void removeIfExists(const uint256& hash);
        std::vector<uint256> getTxsMissing(const uint256&) const;

    private:
        struct ActiveBuilder {
            ThinBlockBuilder builder;
            std::set<ThinBlockWorker*> workers;
        };
        std::map<uint256, ActiveBuilder> builders;
        std::auto_ptr<ThinBlockFinishedCallb> finishedCallb;

        void finishBlock(const uint256& h);
};

class ThinBlockWorker : boost::noncopyable {
    public:
        ThinBlockWorker(ThinBlockManager& m, const uint256& block, NodeId);

        ~ThinBlockWorker();

        bool addTx(const CTransaction& tx);
        std::string blockStr() const;
        uint256 blockHash() const;
        void setAvailable();
        bool isAvailable() const;
        std::vector<uint256> getTxsMissing() const;
        NodeId nodeID() { return node; }
        virtual void buildStub(const CMerkleBlock& m, const TxFinder& txFinder);
        virtual bool isStubBuilt() const;
        void setToWork(const uint256& block);

    private:
        ThinBlockManager& manager;
        uint256 block;
        // if we-re currently re-requesting txs for thin block it provided us
        bool isReRequesting;
        NodeId node;
};

struct thinblock_error : public std::runtime_error {
    thinblock_error(const std::string& e) : std::runtime_error(e) { }
    virtual ~thinblock_error() throw() { }
};

#endif
