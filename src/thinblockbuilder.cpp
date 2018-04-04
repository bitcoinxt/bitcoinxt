#include "thinblockbuilder.h"
#include "thinblock.h"
#include "util.h"
#include "merkleblock.h"
#include "xthin.h"
#include "uint256.h"
#include "blockencodings.h"

#include <unordered_set>
#include <utility>
#include <future>

ThinBlockBuilder::ThinBlockBuilder(const CBlockHeader& header,
        const std::vector<ThinTx>& txs, const TxFinder& txFinder) :
    wanted(txs),
    missing(txs.size())
{
    thinBlock.nVersion = header.nVersion;
    thinBlock.nBits = header.nBits;
    thinBlock.nNonce = header.nNonce;
    thinBlock.nTime = header.nTime;
    thinBlock.hashMerkleRoot = header.hashMerkleRoot;
    thinBlock.hashPrevBlock = header.hashPrevBlock;

    missing = wanted.size();
    for (const ThinTx& h : wanted) {
        CTransaction tx = txFinder(h);
        if (!tx.IsNull())
            --missing;

        // keep null txs, we'll download them later.
        thinBlock.vtx.push_back(tx);
    }
    updateWantedIndex();
    LogPrint(Log::BLOCK, "%d out of %d txs missing\n", missing, wanted.size());
}

void ThinBlockBuilder::updateWantedIndex()
{
    for (auto w = begin(wanted); w != end(wanted); ++w) {
        if (!w->hasShortid())
            continue;

        wantedIdks.insert(w->shortidIdk());
        wantedIndex.insert({w->shortid(), w});
    }
}

ThinBlockBuilder::TXAddRes ThinBlockBuilder::addTransaction(const CTransaction& tx) {
    assert(!tx.IsNull());

    auto loc = end(wanted);

    // Look it up in the shortid index
    for (auto& w : wantedIdks) {

        uint64_t shortid = GetShortID(w, tx.GetHash());
        auto i = wantedIndex.find(shortid);
        if (i == end(wantedIndex))
            continue;

        loc = i->second;
        break;
    }

    if (loc == end(wanted)) {

        // Didn't match any by shortid, try full and cheap matches.
        loc = std::find_if(begin(wanted), end(wanted), [&](const ThinTx& b) {

                // we already checked shortid
                if (b.hasShortid())
                    return false;

                if (b.hasFull())
                    return b.full() == tx.GetHash();

                if (b.hasCheap())
                    return b.cheap() == tx.GetHash().GetCheapHash();

                return false;
        });
    }

    if (loc == end(wanted)) {
        // TX does not belong to block
        return TX_UNWANTED;
    }

    size_t offset = std::distance(begin(wanted), loc);

    if (!thinBlock.vtx[offset].IsNull()) {
        // We already have this one.
        return TX_DUP;
    }

    thinBlock.vtx[offset] = tx;
    missing--;
    return TX_ADDED;
}

int ThinBlockBuilder::numTxsMissing() const {
    return missing;
}

std::vector<std::pair<int, ThinTx> > ThinBlockBuilder::getTxsMissing() const {
    assert(wanted.size() == thinBlock.vtx.size());

    std::vector<std::pair<int, ThinTx> > missing;

    for (size_t i = 0; i < wanted.size(); ++i)
    {
        if (thinBlock.vtx[i].IsNull())
            missing.push_back({i, wanted[i]});
    }
    assert(missing.size() == this->missing);
    return missing;
}

CBlock ThinBlockBuilder::finishBlock() {
    if (numTxsMissing())
        throw thinblock_error("TXs missing. Can't build thin block.");

    for (size_t i = 0; i < thinBlock.vtx.size(); ++i)
        assert(!thinBlock.vtx[i].IsNull());

    bool dummy;
    const uint256& root = thinBlock.BuildMerkleTree(&dummy);


    if (root != thinBlock.hashMerkleRoot) {
        std::stringstream ss;
        ss << "Consistency check failure on attempt to reconstruct thin block. "
            << "Expected merkele root hash " << thinBlock.hashMerkleRoot.ToString()
            << ", got " << root.ToString();
        throw thinblock_error(ss.str());
    }

    LogPrintf("reassembled thin block for %s (%d bytes)\n",
            thinBlock.GetHash().ToString(),
              GetSerializeSize(thinBlock, SER_NETWORK, thinBlock.nVersion));


    CBlock block = thinBlock;
    return block;
}

void ThinBlockBuilder::replaceWantedTx(const std::vector<ThinTx>& tx) {
    assert(!tx.empty());

    if (tx.size() != wanted.size())
        throw thinblock_error("transactions in stub do not match previous stub provided");

    for (size_t i = 0; i < tx.size(); ++i) {
        if (wanted[i].hasCheap() && tx[i].hasCheap()
                && (tx[i].cheap() != wanted[i].cheap()))
            throw thinblock_error("txhash mismatch between provided stubs");
    }

    for (size_t i = 0; i < tx.size(); ++i)
        wanted[i].merge(tx[i]);

    updateWantedIndex();
}
