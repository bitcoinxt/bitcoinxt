// Copyright (c) 2016 The Bitcoin XT developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef BITCOIN_THINBLOCK_H
#define BITCOIN_THINBLOCK_H

#include <boost/noncopyable.hpp>
#include "uint256.h"
#include <stdexcept>

class CNode;
class CInv;
class CTransaction;
class ThinBlockManager;
typedef int NodeId;
class CBlockHeader;

// We may, or may not have the full uint256 hash of transactions in block.
struct ThinTx {
    ThinTx(const uint256& h) :
        hash(h), cheapHash(h.GetCheapHash())  { }

    ThinTx(const uint64_t& h) : cheapHash(h) { }

    bool hasFull() const {
        return !hash.IsNull();
    }

    const uint256& full() const {
        if (!hasFull())
            throw std::runtime_error("full hash not available");
        return hash;
    }
    const uint64_t& cheap() const { return cheapHash; }

    bool operator==(const ThinTx& b) const {
        if (b.hasFull() && this->hasFull())
            return b.full() == this->full();

        return b.cheap() == this->cheap();
    }

    bool operator!=(const ThinTx& b) const {
        return !(b == *this);
    }

    private:
        uint256 hash;
        uint64_t cheapHash;
};

struct StubData {
    virtual ~StubData() = 0;

    virtual CBlockHeader header() const = 0;

    // List of all transactions in block
    virtual std::vector<ThinTx> allTransactions() const = 0;

    // Transactions provded in the stub, if any.
    virtual std::vector<CTransaction> missingProvided() const = 0;
};
inline StubData::~StubData() { }

struct TxFinder {
    // returned tx is null if not found (and needs to be downloaded)
    virtual CTransaction operator()(const ThinTx& hash) const = 0;
    virtual ~TxFinder() = 0;
};
inline TxFinder::~TxFinder() { }

struct thinblock_error : public std::runtime_error {
    thinblock_error(const std::string& e) : std::runtime_error(e) { }
    virtual ~thinblock_error() throw() { }
};

// Each peer we're connected to can work on one thin block
// at a time. This keeps track of the thin block a peer is working on.
class ThinBlockWorker : boost::noncopyable {
    public:
        ThinBlockWorker(ThinBlockManager& mg, NodeId);

        virtual ~ThinBlockWorker();

        virtual bool addTx(const CTransaction& tx);
        virtual std::vector<ThinTx> getTxsMissing() const;

        virtual void setAvailable();
        virtual bool isAvailable() const;

        virtual void buildStub(const StubData&, const TxFinder&);
        virtual bool isStubBuilt() const;
        virtual void setToWork(const uint256& block);
        virtual bool isOnlyWorker() const;

        // Request block. Implementation may append their request to
        // getDataReq or implement a more specialized behavour.
        // Method is called during ProcessGetData.
        virtual void requestBlock(const uint256& block,
                std::vector<CInv>& getDataReq, CNode& node) = 0;

        bool isReRequesting() const;
        void setReRequesting(bool);

        uint256 blockHash() const;
        std::string blockStr() const;

        NodeId nodeID() const { return node; }

    private:
        ThinBlockManager& mg;
        // if we're re-requesting txs for the block worker
        // provided us.
        bool rerequesting;
        NodeId node;
        uint256 block;
};

#endif
