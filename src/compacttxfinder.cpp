// Copyright (c) 2016 The Bitcoin XT developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#include "compacttxfinder.h"
#include "util.h"
#include "txmempool.h"
#include "blockencodings.h" // GetShortID

CompactTxFinder::CompactTxFinder(const CTxMemPool& m,
        uint64_t idk0, uint64_t idk1) : mempool(m) {
    initMapping(idk0, idk1);
}

void CompactTxFinder::initMapping(uint64_t idk0, uint64_t idk1) {

    std::vector<uint256> hashes;
    mempool.queryHashes(hashes);

    for (auto& t : hashes) {
        uint64_t shortID = GetShortID(idk0, idk1, t);

        if (mappedMempool.count(shortID)) {

            LogPrint("thin", "ShortID hash collision in mempool");

            // Erase, so the tx re-fetched from peer instead.
            mappedMempool.erase(shortID);
            continue;
        }
        mappedMempool[shortID] = t;
    }
}

CTransaction CompactTxFinder::operator()(const ThinTx& hash) const {

    CTransaction match;

    if (!mappedMempool.count(hash.obfuscated()))
        return match;

    uint256 realHash = mappedMempool.find(hash.obfuscated())->second;

    // Tx may not exist anymore in mempool.
    // Lookup leaves match empty if it does not.
    mempool.lookup(realHash, match);
    return match;
}

