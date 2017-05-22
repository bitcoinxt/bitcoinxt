// Copyright (c) 2016 The Bitcoin XT developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef BITCOIN_THINBLOCK_H
#define BITCOIN_THINBLOCK_H

#include <boost/noncopyable.hpp>
#include "uint256.h"
#include <stdexcept>
#include <set>

class CNode;
class CInv;
class CTransaction;
class ThinBlockManager;
typedef int NodeId;
class CBlockHeader;

// Transactions IDs are in different format for each thin block implemenation.
// ThisTx is an encapsulation for all the formats.
class ThinTx {
public:
    ThinTx(const uint256& full);
    ThinTx(const uint64_t& cheap);
    ThinTx(const uint64_t& obfuscated,
            const uint64_t& idk0, const uint64_t& idk1);
    static ThinTx Null() { return ThinTx(uint256()); }

    // If it is known that tx is the same as this
    // one, grab any additional info possible.
    void merge(const ThinTx& tx);

    bool hasFull() const;
    const uint256& full() const;
    const uint64_t& cheap() const;
    uint64_t obfuscated() const;
    bool hasCheap() const { return cheap_ != 0; }

    bool isNull() const {
        return !hasFull() && !hasCheap() && !hasObfuscated();
    }

    bool equals(const ThinTx& b) const;
    bool equals(const uint256& b) const;

private:

    uint256 full_; //< Bundled/prefilled transactions have known full hash.
    uint64_t cheap_; //< Used by xthin
    struct {
        uint64_t id;
        uint64_t idk0;
        uint64_t idk1;
    } obfuscated_; //< Used by compact thin

    bool hasObfuscated() const;

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

class TxFinder {
public:
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

        virtual bool addTx(const uint256& block, const CTransaction& tx);

        virtual std::vector<ThinTx> getTxsMissing(const uint256&) const;

        virtual void buildStub(const StubData&, const TxFinder&);
        virtual bool isStubBuilt(const uint256& block) const;
        virtual void addWork(const uint256& block);
        virtual void stopWork(const uint256& block);
        virtual void stopAllWork();
        virtual bool isOnlyWorker(const uint256& block) const;

        // Request block. Implementation may append their request to
        // getDataReq or implement a more specialized behavour.
        // Method is called during ProcessGetData.
        virtual void requestBlock(const uint256& block,
                std::vector<CInv>& getDataReq, CNode& node) = 0;

        bool isReRequesting(const uint256& block) const;
        void setReRequesting(const uint256& block, bool);

        NodeId nodeID() const { return node; }

        bool isWorking() const {
            return !blocks.empty();
        }

        virtual bool isWorkingOn(const uint256& h) const {
            return blocks.count(h);
        }

    private:
        ThinBlockManager& mg;
        NodeId node;
        // blocks this worker is downloading
        std::set<uint256> blocks;
        // block we're downloading and re-requesting transcations for
        std::set<uint256> rerequesting;
};

#endif
