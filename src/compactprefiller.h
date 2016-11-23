// Copyright (c) 2016 The Bitcoin XT developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef BITCOIN_COMPACT_PREFILLER_H
#define BITCOIN_COMPACT_PREFILLER_H

#include <memory>
#include "serialize.h"
#include "primitives/transaction.h"

class CBlock;
class CNode;
class CTransaction;
class CRollingBloomFilter;

// When sending a compact block to a peer, one should attempt
// to prefill transactions that they are likely to be missing.

// Dumb helper to handle CTransaction compression at serialize-time
struct TransactionCompressor {
private:
    CTransaction& tx;
public:
    TransactionCompressor(CTransaction& txIn) : tx(txIn) {}

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        READWRITE(tx); //TODO: Compress tx encoding
    }
};

struct PrefilledTransaction {
    // Used as an offset since last prefilled tx in CompactBlock,
    uint16_t index;
    CTransaction tx;

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        uint64_t idx = index;
        READWRITE(COMPACTSIZE(idx));
        if (idx > std::numeric_limits<uint16_t>::max())
            throw std::ios_base::failure("index overflowed 16-bits");
        index = idx;
        READWRITE(REF(TransactionCompressor(tx)));
    }
};

class CompactPrefiller {
    public:
        virtual std::vector<PrefilledTransaction> fillFrom(
                const CBlock&) const = 0;

        virtual ~CompactPrefiller() = 0;
};
inline CompactPrefiller::~CompactPrefiller() { }

// Sends coinebase only. This is the same strategy that Core deploys in
// Bitcoin Core 0.13. This strategy causes the most re-request round trips.
class CoinbaseOnlyPrefiller : public CompactPrefiller {
    public:
        std::vector<PrefilledTransaction> fillFrom(const CBlock&) const override;
};

// This prefill strategy uses the inventory known filter to determine
// what transactions the peer is missing.
class InventoryKnownPrefiller : public CompactPrefiller {
    public:
        InventoryKnownPrefiller(std::unique_ptr<CRollingBloomFilter> inventoryKnown);
        ~InventoryKnownPrefiller();

        std::vector<PrefilledTransaction> fillFrom(const CBlock&) const override;
    private:
        const std::unique_ptr<CRollingBloomFilter> inventoryKnown;
};

// Since we're guessing what txs the peer needs, we need to pick a sane strategy
// for each particular peer.
std::unique_ptr<CompactPrefiller> choosePrefiller(CNode&);

#endif
