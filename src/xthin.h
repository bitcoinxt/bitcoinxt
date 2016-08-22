// Copyright (c) 2016- The Bitcoin XT developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef BITCOIN_XTHIN_H
#define BITCOIN_XTHIN_H

#include "thinblock.h"
#include "serialize.h"
#include "primitives/block.h"
#include "util.h"
#include <memory>
#include <stdexcept>

class CBloomFilter;

// thrown in the extremely unlikely event of cheap hash collision
struct xthin_collision_error : public std::runtime_error {
    xthin_collision_error() : std::runtime_error("xthin collision error") { }
};

// Specialized thin block (BUIP010). Contains list of transactions as uint64_t
// rather that uint256.
//
// Bundles transactions that remote node is believed to be missing
// based on a filter it sent us.
//
// Always includes coinbase.
class XThinBlock {

    public:
        XThinBlock();
        XThinBlock(const CBlock&, const CBloomFilter&, bool checkCollision = true);
        ADD_SERIALIZE_METHODS;

        CBlockHeader header;
        std::vector<uint64_t> txHashes; // all transactions in the block
        std::vector<CTransaction> missing; // transactions that were missing

        // primitive check to see if block is valid
        // throws on error
        void selfValidate() const;

        template <typename Stream, typename Operation>
        inline void SerializationOp(Stream& s,
                Operation ser_action, int nType, int nVersion)
        {
            READWRITE(header);
            READWRITE(txHashes);
            READWRITE(missing);
        }
};

class XThinWorker : public ThinBlockWorker {

    public:
        XThinWorker(ThinBlockManager&, NodeId,
                std::unique_ptr<struct TxHashProvider>);

        // only for unit testing
        XThinWorker(ThinBlockManager&, NodeId);

        void requestBlock(const uint256& block,
                std::vector<CInv>& getDataReq, CNode& node) override;

    private:
        std::unique_ptr<struct TxHashProvider> HashProvider;

};

// Functor for providing list of transactions that
// we already have (and don't need when requesting a thin block)
struct TxHashProvider {
    virtual void operator()(std::vector<uint256>& dst) = 0;
    virtual ~TxHashProvider() = 0;
};
inline TxHashProvider::~TxHashProvider() { }

struct XThinStub : public StubData {
    XThinStub(const XThinBlock& b) : xblock(b) {
        try {
            xblock.selfValidate();
        }
        catch (const std::exception& e) {
            throw thinblock_error(e.what());
        }

        LogPrint("thin", "Created xthin stub for %s, %d transactions.\n",
                header().GetHash().ToString(), allTransactions().size());
    }

    virtual CBlockHeader header() const {
        return xblock.header;
    }

    // List of all transactions in block
    virtual std::vector<ThinTx> allTransactions() const {
        std::vector<ThinTx> txs(
                xblock.txHashes.begin(),
                xblock.txHashes.end());
        return txs;
    }

    // Transactions provded in the stub, if any.
    virtual std::vector<CTransaction> missingProvided() const {
        return xblock.missing;
    }

    private:
        XThinBlock xblock;
};

// XThin has its own message for re-requesting transactions missing.
class XThinReRequest {
public:
    uint256 block;
    std::set<uint64_t> txRequesting;

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
        inline void SerializationOp(Stream& s,
                Operation ser_action, int nType, int nVersion)
    {
        READWRITE(block);
        READWRITE(txRequesting);
    }
};

// This is the reponse to a xthin re-request for transactions.
struct XThinReReqResponse {

    XThinReReqResponse() { }
    XThinReReqResponse(const CBlock& srcBlock, const std::set<uint64_t>& requesting);

    uint256 block;
    std::vector<CTransaction> txRequested;

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
        inline void SerializationOp(Stream& s,
                Operation ser_action, int nType, int nVersion)
    {
        READWRITE(block);
        READWRITE(txRequested);
    }
};

#endif
