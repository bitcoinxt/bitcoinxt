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
    typedef std::vector<ThinTx>::const_iterator auto_;
    for (auto_ h = wantedTxs.begin(); h != wantedTxs.end(); ++h) {
        CTransaction tx = txFinder(*h);
        if (!tx.IsNull())
            --missing;

        // keep null txs, we'll download them later.
        // coinbase is guaranteed to be missing
        thinBlock.vtx.push_back(tx);
    }
    LogPrint("thin", "%d out of %d txs missing\n", missing, wantedTxs.size());
}

ThinBlockBuilder::TXAddRes ThinBlockBuilder::addTransaction(const CTransaction& tx) {
    assert(!tx.IsNull());
    typedef std::vector<ThinTx>::iterator auto_;
    auto_ loc = std::find(
            wantedTxs.begin(), wantedTxs.end(), ThinTx(tx.GetHash()));

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

std::vector<ThinTx> ThinBlockBuilder::getTxsMissing() const {
    assert(wantedTxs.size() == thinBlock.vtx.size());

    std::vector<ThinTx> missing;

    for (size_t i = 0; i < wantedTxs.size(); ++i)
        if (thinBlock.vtx[i].IsNull())
            missing.push_back(wantedTxs[i]);

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

    if (wantedTxs.at(0).hasFull())
        return;

    if (!tx.at(0).hasFull())
        return;

    if (tx.size() != wantedTxs.size())
        throw thinblock_error("transactions in stub do not match previous stub provided");

    for (size_t i = 0; i < tx.size(); ++i)
        if (tx[i].cheap() != wantedTxs[i].cheap())
            throw thinblock_error("txhash mismatch between provided stubs");

    wantedTxs = tx;
}
