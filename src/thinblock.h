// Copyright (c) 2016 The Bitcoin XT developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef BITCOIN_THINBLOCK_H
#define BITCOIN_THINBLOCK_H

#include <boost/noncopyable.hpp>
#include "uint256.h"
#include <stdexcept>
#include <set>
#include <memory>

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
    // assumes !full.IsNull()
    ThinTx(const uint256& full);
    ThinTx(const uint64_t& cheap);
    ThinTx(const uint64_t& shortid, const std::pair<uint64_t, uint64_t>& idk);
    // assumes !full.IsNull()
    ThinTx(const uint256& full, const std::pair<uint64_t, uint64_t>& idk);
    static ThinTx Null() { return ThinTx(uint64_t(0)); }

    // If it is known that tx is the same as this
    // one, grab any additional info possible.
    void merge(const ThinTx& tx);

    bool hasFull() const { return hasFull_; }
    const uint256& full() const { return full_; }

    bool hasCheap() const { return cheap_ != 0 || hasFull(); }
    uint64_t cheap() const;

    bool hasShortid() const { return shortid_.id != 0; }
    uint64_t shortid() const { return shortid_.id; }
    const std::pair<uint64_t, uint64_t>& shortidIdk() const {
        return shortid_.idk;
    }

    bool isNull() const {
        return !hasFull() && !hasCheap() && !hasShortid();
    }

    bool equals(const ThinTx& b) const;

private:
    // Bundled/prefilled transactions have known full hash
    uint256 full_;
    bool hasFull_ = false;
    // Used by xthin, mutable to allow lazy creation
    mutable uint64_t cheap_ = 0;
    // Used by compact blocks
    struct {
        uint64_t id;
        std::pair<uint64_t, uint64_t> idk;
    } shortid_;
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

class BlockAnnHandle {
    public:
        virtual NodeId nodeID() const = 0;
        virtual ~BlockAnnHandle() = 0;
};
inline BlockAnnHandle::~BlockAnnHandle() { }

// Each peer we're connected to can work on one thin block
// at a time. This keeps track of the thin block a peer is working on.
class ThinBlockWorker : boost::noncopyable {
    public:
        ThinBlockWorker(ThinBlockManager& mg, NodeId);

        virtual ~ThinBlockWorker();

        virtual bool addTx(const uint256& block, const CTransaction& tx);

        virtual std::vector<std::pair<int, ThinTx> > getTxsMissing(const uint256&) const;

        virtual void buildStub(const StubData&, const TxFinder&, CNode& from);
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

        // Enables block announcements with thin blocks.
        // Returns a RAII object that disables them on destruct.
        // Returns nullptr if peer does ont support this.
        virtual std::unique_ptr<BlockAnnHandle> requestBlockAnnouncements(CNode&)
        {
            return nullptr;
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
