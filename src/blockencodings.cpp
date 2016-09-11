// Copyright (c) 2016 The Bitcoin Core developers
// Copyright (c) 2016 The Bitcoin XT developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "blockencodings.h"
#include "consensus/consensus.h"
#include "consensus/validation.h"
#include "chainparams.h"
#include "hash.h"
#include "random.h"
#include "streams.h"
#include "txmempool.h"
#include "main.h"
#include "util.h"

#include <unordered_map>

uint64_t GetShortID(
        const uint64_t& shorttxidk0,
        const uint64_t& shorttxidk1,
        const uint256& txhash)
{
    static_assert(CompactBlock::SHORTTXIDS_LENGTH == 6, "shorttxids calculation assumes 6-byte shorttxids");
    return SipHashUint256(shorttxidk0, shorttxidk1, txhash) & 0xffffffffffffL;
}

#define MIN_TRANSACTION_SIZE (::GetSerializeSize(CTransaction(), SER_NETWORK, PROTOCOL_VERSION))

CompactBlock::CompactBlock(const CBlock& block, const CompactPrefiller& prefiller) :
        nonce(GetRand(std::numeric_limits<uint64_t>::max())), header(block)
{
    FillShortTxIDSelector();

    if (block.vtx.empty())
        throw std::invalid_argument(__func__ + std::string(" expects coinbase tx"));

    prefilledtxn = prefiller.fillFrom(block);

    auto isPrefilled = [this](const uint256& tx) {
        for (auto& p : this->prefilledtxn)
            if (p.tx.GetHash() == tx)
                return true;
        return false;
    };

    // Fill short IDs.
    for (const CTransaction& tx : block.vtx) {
        if (isPrefilled(tx.GetHash()))
            continue;

        shorttxids.push_back(GetShortID(tx.GetHash()));
    }
}

void CompactBlock::FillShortTxIDSelector() const {
    CDataStream stream(SER_NETWORK, PROTOCOL_VERSION);
    stream << header << nonce;
    CSHA256 hasher;
    hasher.Write((unsigned char*)&(*stream.begin()), stream.end() - stream.begin());
    uint256 shorttxidhash;
    hasher.Finalize(shorttxidhash.begin());
    shorttxidk0 = shorttxidhash.GetUint64(0);
    shorttxidk1 = shorttxidhash.GetUint64(1);
}

uint64_t CompactBlock::GetShortID(const uint256& txhash) const {
    return ::GetShortID(shorttxidk0, shorttxidk1, txhash);
}

void validateCompactBlock(const CompactBlock& cmpctblock) {
    if (cmpctblock.header.IsNull() || (cmpctblock.shorttxids.empty() && cmpctblock.prefilledtxn.empty()))
        throw std::invalid_argument("empty data in compact block");

    if (cmpctblock.shorttxids.size() + cmpctblock.prefilledtxn.size() > MAX_BLOCK_SIZE / MIN_TRANSACTION_SIZE)
        throw std::invalid_argument("compact block exceeds max txs in a block");

    int32_t lastprefilledindex = -1;
    for (size_t i = 0; i < cmpctblock.prefilledtxn.size(); i++) {
        if (cmpctblock.prefilledtxn[i].tx.IsNull())
            throw std::invalid_argument("null tx in compact block");

        lastprefilledindex += cmpctblock.prefilledtxn[i].index + 1; //index is a uint16_t, so cant overflow here
        if (lastprefilledindex > std::numeric_limits<uint16_t>::max())
            throw std::invalid_argument("tx index overflows");

        if (static_cast<uint32_t>(lastprefilledindex) > cmpctblock.shorttxids.size() + i) {
            // If we are inserting a tx at an index greater than our full list of shorttxids
            // plus the number of prefilled txn we've inserted, then we have txn for which we
            // have neither a prefilled txn or a shorttxid!
            throw std::invalid_argument("invalid index for tx");
        }
    }
}
