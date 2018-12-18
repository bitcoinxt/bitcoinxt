#include "miner/serializableblockbuilder.h"

#include "consensus/validation.h"
#include "utildebug.h"
#include "main.h" // TestBlockValidity

namespace miner {

SerializableBlockBuilder::SerializableBlockBuilder() :
    useLTOR(true), isFinalized(false), coinbase(nullptr, 0, 0)
{
    block.nNonce = 0;
}

void SerializableBlockBuilder::SetTime(uint32_t t) {
    block.nTime = t;
}

uint32_t SerializableBlockBuilder::GetTime() const {
    return block.GetBlockTime();
}

void SerializableBlockBuilder::SetVersion(uint32_t v) {
    block.nVersion = v;
}

uint32_t SerializableBlockBuilder::GetVersion() const {
    return block.nVersion;
}

void SerializableBlockBuilder::SetCoinbase(BuilderEntry tx) {
    THROW_UNLESS(!coinbase.IsValid());
    THROW_UNLESS(tx.IsValid());
    THROW_UNLESS(tx.IsCoinBase());
    coinbase = std::move(tx);
}

void SerializableBlockBuilder::AddTx(BuilderEntry tx) {
    THROW_UNLESS(tx.IsValid());
    THROW_UNLESS(!tx.IsCoinBase());
    txs.emplace_back(std::move(tx));
}

void SerializableBlockBuilder::SetBits(uint32_t bits) {
    block.nBits = bits;
}

void SerializableBlockBuilder::SetHashPrevBlock(const uint256& hash) {
    block.hashPrevBlock = hash;
}

void SerializableBlockBuilder::DisableLTOR() {
    useLTOR = false;
}

void SerializableBlockBuilder::Finalize(const Consensus::Params&) {
    THROW_UNLESS(!isFinalized);
    THROW_UNLESS(block.nTime);
    THROW_UNLESS(block.nVersion);
    THROW_UNLESS(block.vtx.empty());
    THROW_UNLESS(!block.hashPrevBlock.IsNull());
    THROW_UNLESS(coinbase.IsCoinBase());

    if (useLTOR) {
        std::sort(std::begin(txs), std::end(txs), EntryHashCmp);
    }

    block.vtx.reserve(txs.size() + 1);
    block.vtx.push_back(coinbase.GetTx());

    for (auto& tx : txs) {
        block.vtx.push_back(tx.GetTx());
    }

    isFinalized = true;
}

void SerializableBlockBuilder::CheckValidity(CBlockIndex* pindexPrev) {
    CValidationState state;
    if (!TestBlockValidity(state, block, pindexPrev, false, false)) {
        std::stringstream err;
        err << __func__ << ": TestBlockValidity failed: "
            << FormatStateMessage(state);
        throw std::runtime_error(err.str());
    }
}

CBlock SerializableBlockBuilder::Release() {
    THROW_UNLESS(isFinalized);
    return std::move(block);
}

} // ns miner
