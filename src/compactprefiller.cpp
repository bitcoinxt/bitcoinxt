// Copyright (c) 2016 The Bitcoin XT developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#include "compactprefiller.h"
#include "bloom.h"
#include "net.h" // for CNode
#include "primitives/block.h"
#include "serialize.h"
#include "sync.h"
#include "version.h"
#include "util.h"

std::vector<PrefilledTransaction> CoinbaseOnlyPrefiller::fillFrom(
        const CBlock& block) const {
    return { PrefilledTransaction{0, block.vtx[0]} };
};

std::vector<PrefilledTransaction> InventoryKnownPrefiller::fillFrom(
        const CBlock& block) const {

    std::vector<PrefilledTransaction> filled;

    size_t prevIndex = 0;
    filled.push_back(PrefilledTransaction{0, block.vtx[0]});

    const unsigned MAX_PREFILLED_SIZE = 10 * 1000; // 10KB
    unsigned int txsSize = ::GetSerializeSize(
            filled[0].tx, SER_NETWORK, PROTOCOL_VERSION);

    for (size_t i = 1; i < block.vtx.size(); i++) {
        const CTransaction& tx = block.vtx[i];

        if (inventoryKnown->contains(tx.GetHash()))
            continue;

        txsSize += ::GetSerializeSize(tx, SER_NETWORK, PROTOCOL_VERSION);
        if (txsSize > MAX_PREFILLED_SIZE) {
            // Appending more would exceed MAX_PREFILLED_SIZE,
            // just return what we have.
            return filled;
        }

        filled.push_back(PrefilledTransaction{
                static_cast<uint16_t>(i - (prevIndex + 1)), tx});
        prevIndex = i;
    }
    return filled;
}

InventoryKnownPrefiller::InventoryKnownPrefiller(
        std::unique_ptr<CRollingBloomFilter> inventoryKnown) :
    inventoryKnown(std::move(inventoryKnown))
{
}

InventoryKnownPrefiller::~InventoryKnownPrefiller() { }

std::unique_ptr<CompactPrefiller> choosePrefiller(CNode& node) {

    if (!node.fRelayTxes) {
        // Peer does not want us to relay transactions to it.
        // We cannot assume anything about this peers mempool state.
        // Fall back to the dumbest strategy.
        return std::unique_ptr<CompactPrefiller>(new CoinbaseOnlyPrefiller);
    }

    // Ideas for future:
    // * Strategy to prefill transactions that were not in our own mempool
    //   at the time we received the block.
    // * Prefill RBF transactions for non-Core nodes. A replaced RBP tx is
    //   likely to be ignored as duplicate by all except for Core.

    LOCK(node.cs_inventory);
    std::unique_ptr<CRollingBloomFilter> copy(
            new CRollingBloomFilter(node.filterInventoryKnown));
    return std::unique_ptr<CompactPrefiller>(new InventoryKnownPrefiller(std::move(copy)));
}
