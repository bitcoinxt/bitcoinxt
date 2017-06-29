#include "thinblockbuilder.h"
#include "thinblock.h"
#include "util.h"
#include "merkleblock.h"
#include "xthin.h"
#include "uint256.h"

ThinBlockBuilder::ThinBlockBuilder(const CBlockHeader& header,
        const std::vector<ThinTx>& txs, const TxFinder& txFinder) :
    wantedTxs(txs),
    missing(txs.size())
{
    thinBlock.nVersion = header.nVersion;
    thinBlock.nBits = header.nBits;
    thinBlock.nNonce = header.nNonce;
    thinBlock.nTime = header.nTime;
    thinBlock.hashMerkleRoot = header.hashMerkleRoot;
    thinBlock.hashPrevBlock = header.hashPrevBlock;

    missing = wantedTxs.size();
    for (const ThinTx& h : wantedTxs) {
        CTransaction tx = txFinder(h);
        if (!tx.IsNull())
            --missing;

        // keep null txs, we'll download them later.
        thinBlock.vtx.push_back(tx);
    }
    LogPrint("thin", "%d out of %d txs missing\n", missing, wantedTxs.size());
}

ThinBlockBuilder::TXAddRes ThinBlockBuilder::addTransaction(const CTransaction& tx) {
    assert(!tx.IsNull());

    auto loc = std::find_if(wantedTxs.begin(), wantedTxs.end(), [&tx](const ThinTx& b) {
        return b.equals(tx.GetHash());
    });

    if (loc == wantedTxs.end()){
        // TX does not belong to block
        return TX_UNWANTED;
    }

    size_t offset = std::distance(wantedTxs.begin(), loc);

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
    assert(wantedTxs.size() == thinBlock.vtx.size());

    std::vector<std::pair<int, ThinTx> > missing;

    for (size_t i = 0; i < wantedTxs.size(); ++i)
        if (thinBlock.vtx[i].IsNull())
            missing.push_back(std::make_pair(i, wantedTxs[i]));

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
            thinBlock.GetSerializeSize(SER_NETWORK, thinBlock.nVersion));


    CBlock block = thinBlock;
    return block;
}

void ThinBlockBuilder::replaceWantedTx(const std::vector<ThinTx>& tx) {
    assert(!tx.empty());

    if (tx.size() != wantedTxs.size())
        throw thinblock_error("transactions in stub do not match previous stub provided");

    for (size_t i = 0; i < tx.size(); ++i) {
        if (wantedTxs[i].hasCheap() && tx[i].hasCheap()
                && (tx[i].cheap() != wantedTxs[i].cheap()))
            throw thinblock_error("txhash mismatch between provided stubs");
    }

    for (size_t i = 0; i < tx.size(); ++i)
        wantedTxs[i].merge(tx[i]);
}
