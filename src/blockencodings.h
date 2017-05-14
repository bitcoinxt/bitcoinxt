// Copyright (c) 2016 The Bitcoin Core developers
// Copyright (c) 2016 The Bitcoin XT developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_BLOCK_ENCODINGS_H
#define BITCOIN_BLOCK_ENCODINGS_H

#include "primitives/block.h"
#include "compactprefiller.h"

#include <memory>

uint64_t GetShortID(
        const uint64_t& shorttxidk0,
        const uint64_t& shorttxidk1,
        const uint256& txhash);

class CTxMemPool;

class CompactReRequest {
public:
    // A CompactReRequest message
    uint256 blockhash;
    std::vector<uint16_t> indexes;

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        READWRITE(blockhash);
        uint64_t indexes_size = (uint64_t)indexes.size();
        READWRITE(COMPACTSIZE(indexes_size));
        if (ser_action.ForRead()) {
            size_t i = 0;
            while (indexes.size() < indexes_size) {
                indexes.resize(std::min((uint64_t)(1000 + indexes.size()), indexes_size));
                for (; i < indexes.size(); i++) {
                    uint64_t index = 0;
                    READWRITE(COMPACTSIZE(index));
                    if (index > std::numeric_limits<uint16_t>::max())
                        throw std::ios_base::failure("index overflowed 16 bits");
                    indexes[i] = index;
                }
            }

            uint16_t offset = 0;
            for (size_t i = 0; i < indexes.size(); i++) {
                if (uint64_t(indexes[i]) + uint64_t(offset) > std::numeric_limits<uint16_t>::max())
                    throw std::ios_base::failure("indexes overflowed 16 bits");
                indexes[i] = indexes[i] + offset;
                offset = indexes[i] + 1;
            }
        } else {
            for (size_t i = 0; i < indexes.size(); i++) {
                uint64_t index = indexes[i] - (i == 0 ? 0 : (indexes[i - 1] + 1));
                READWRITE(COMPACTSIZE(index));
            }
        }
    }
};

class CompactReReqResponse {
public:
    uint256 blockhash;
    std::vector<CTransaction> txn;

    CompactReReqResponse() {}
    CompactReReqResponse(const CompactReRequest& req) :
        blockhash(req.blockhash), txn(req.indexes.size()) {}

    CompactReReqResponse(const CBlock& block, const std::vector<uint16_t>& indexes) {
        blockhash = block.GetHash();
        if (indexes.size() > block.vtx.size())
            throw std::invalid_argument("request more transactions than are in a block");
        for (uint16_t i : indexes) {
            if (i >= block.vtx.size())
                throw std::invalid_argument("out of bound tx in rerequest");
            txn.push_back(block.vtx.at(i));
        }
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        READWRITE(blockhash);
        uint64_t txn_size = (uint64_t)txn.size();
        READWRITE(COMPACTSIZE(txn_size));
        if (ser_action.ForRead()) {
            size_t i = 0;
            while (txn.size() < txn_size) {
                txn.resize(std::min((uint64_t)(1000 + txn.size()), txn_size));
                for (; i < txn.size(); i++)
                    READWRITE(REF(TransactionCompressor(txn[i])));
            }
        } else {
            for (size_t i = 0; i < txn.size(); i++)
                READWRITE(REF(TransactionCompressor(txn[i])));
        }
    }
};

class CompactBlock {
public:
    mutable uint64_t shorttxidk0, shorttxidk1;
private:
    uint64_t nonce;

    void FillShortTxIDSelector() const;

public:
    static const int SHORTTXIDS_LENGTH = 6;

    std::vector<uint64_t> shorttxids;
    std::vector<PrefilledTransaction> prefilledtxn;

    CBlockHeader header;

    // Dummy for deserialization
    CompactBlock() {}

    CompactBlock(const CBlock& block, const CompactPrefiller& prefiller);

    uint64_t GetShortID(const uint256& txhash) const;

    size_t BlockTxCount() const { return shorttxids.size() + prefilledtxn.size(); }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        READWRITE(header);
        READWRITE(nonce);

        uint64_t shorttxids_size = (uint64_t)shorttxids.size();
        READWRITE(COMPACTSIZE(shorttxids_size));
        if (ser_action.ForRead()) {
            size_t i = 0;
            while (shorttxids.size() < shorttxids_size) {
                shorttxids.resize(std::min((uint64_t)(1000 + shorttxids.size()), shorttxids_size));
                for (; i < shorttxids.size(); i++) {
                    uint32_t lsb = 0; uint16_t msb = 0;
                    READWRITE(lsb);
                    READWRITE(msb);
                    shorttxids[i] = (uint64_t(msb) << 32) | uint64_t(lsb);
                    static_assert(SHORTTXIDS_LENGTH == 6, "shorttxids serialization assumes 6-byte shorttxids");
                }
            }
        } else {
            for (size_t i = 0; i < shorttxids.size(); i++) {
                uint32_t lsb = shorttxids[i] & 0xffffffff;
                uint16_t msb = (shorttxids[i] >> 32) & 0xffff;
                READWRITE(lsb);
                READWRITE(msb);
            }
        }

        READWRITE(prefilledtxn);

        if (ser_action.ForRead())
            FillShortTxIDSelector();
    }
};

unsigned int minTxSize();
void validateCompactBlock(const CompactBlock& cmpctblock, uint64_t currMaxBlockSize);

#endif
