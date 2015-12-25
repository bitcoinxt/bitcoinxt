#include "thinblockbuilder.h"
#include "merkleblock.h"
#include "util.h"
#include <utility>
#include <sstream>

ThinBlockBuilder::ThinBlockBuilder(const CMerkleBlock& m, const TxFinder& txFinder) :
    missing(NOT_BUILDING)
{
    thinBlock.nVersion = m.header.nVersion;
    thinBlock.nBits = m.header.nBits;
    thinBlock.nNonce = m.header.nNonce;
    thinBlock.nTime = m.header.nTime;
    thinBlock.hashMerkleRoot = m.header.hashMerkleRoot;
    thinBlock.hashPrevBlock = m.header.hashPrevBlock;
    wantedTxs = getHashes(m);

    missing = wantedTxs.size();
    typedef std::vector<uint256>::const_iterator auto_;
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

std::vector<uint256> ThinBlockBuilder::getHashes(const CMerkleBlock& m) const {

    std::vector<uint256> txHashes;
    // FIXME: Calculate a sane number of max
    // transactions here, or skip the check.
    uint256 merkleRoot = CMerkleBlock(m).txn.ExtractMatches(50000, txHashes);
    if (m.header.hashMerkleRoot != merkleRoot)
        throw thinblock_error("Failed to match Merkle root or bad tree in thin block");

    return txHashes;
}

bool ThinBlockBuilder::isValid() const {
    return missing != NOT_BUILDING;
}

ThinBlockBuilder::TXAddRes ThinBlockBuilder::addTransaction(const CTransaction& tx) {
    assert(isValid());
    assert(!tx.IsNull());
    typedef std::vector<uint256>::iterator auto_;
    auto_ loc = std::find(
            wantedTxs.begin(), wantedTxs.end(), tx.GetHash());

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
    assert(isValid());
    return missing;
}

std::vector<uint256> ThinBlockBuilder::getTxsMissing() const {
    assert(wantedTxs.size() == thinBlock.vtx.size());
    assert(isValid());

    std::vector<uint256> missing;

    for (size_t i = 0; i < wantedTxs.size(); ++i) {
        if (thinBlock.vtx[i].IsNull())
            missing.push_back(wantedTxs[i]);
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
            thinBlock.GetSerializeSize(SER_NETWORK, CBlock::CURRENT_VERSION));


    CBlock block = thinBlock;
    reset();
    return block;
}

void ThinBlockBuilder::reset() {
    missing = NOT_BUILDING;
    thinBlock.SetNull();
    wantedTxs.clear();
}

ThinBlockManager::ThinBlockManager(
        std::auto_ptr<ThinBlockFinishedCallb> callb) :
    finishedCallb(callb.release())
{ }

void ThinBlockManager::addWorker(const uint256& block, ThinBlockWorker& w) {
    if (!builders.count(block))
        builders[block] = ThinBlockManager::ActiveBuilder();
    builders[block].workers.insert(&w);
}

void ThinBlockManager::delWorker(ThinBlockWorker& w) {
    typedef std::map<uint256, ActiveBuilder>::iterator auto_;
    for (auto_ a = builders.begin(); a != builders.end(); ++a) {
        if (!a->second.workers.erase(&w))
            continue;

        if (a->second.workers.empty())
            builders.erase(a);

        return;
    }
}
int ThinBlockManager::numWorkers(const uint256& block) const {
    return builders.count(block)
        ? builders.find(block)->second.workers.size()
        : 0;
}

void ThinBlockManager::buildStub(const CMerkleBlock& m, const TxFinder& txFinder) {
    uint256 h = m.header.GetHash();
    assert(!isStubBuilt(h));
    builders[h].builder = ThinBlockBuilder(m, txFinder);
}

bool ThinBlockManager::isStubBuilt(const uint256& block) const {
    return builders.count(block)
        && builders.find(block)->second.builder.isValid();
}


// Try to add transaction to the block peer is providing.
// Returns true if tx belonged to block
bool ThinBlockManager::addTx(const uint256& block, const CTransaction& tx) {
    ThinBlockBuilder& b = builders[block].builder;

    if (!b.isValid())
        return false;

    ThinBlockBuilder::TXAddRes res = b.addTransaction(tx);

    if (res == ThinBlockBuilder::TX_UNWANTED) {
        LogPrint("thin", "tx %s does not belong to block %s\n",
                tx.GetHash().ToString(), block.ToString());
        return false;
    }

    else if (res == ThinBlockBuilder::TX_DUP)
        LogPrint("thin2", "already had tx %s\n",
                tx.GetHash().ToString());

    else if (res == ThinBlockBuilder::TX_ADDED)
        LogPrint("thin2", "added transaction %s\n", tx.GetHash().ToString());

    else { assert(!"unknown addTransaction response"); }

    if (b.numTxsMissing() == 0)
        finishBlock(block);

    return true;
}

void ThinBlockManager::removeIfExists(const uint256& h) {
    if (!builders.count(h))
        return;

    typedef std::set<ThinBlockWorker*>::iterator auto_;

    // Take copy. Calling setAvailable causes workers to remove
    // themself from set (changing it during iteration)
    std::set<ThinBlockWorker*> workers = builders[h].workers;
    for (auto_ w = workers.begin(); w != workers.end(); ++w)
        (*w)->setAvailable();
    builders.erase(h);
}

std::vector<uint256> ThinBlockManager::getTxsMissing(const uint256& hash) const {
    assert(builders.count(hash));
    return builders.find(hash)->second.builder.getTxsMissing();
}

void ThinBlockManager::finishBlock(const uint256& h) {
    CBlock block;
    try {
        block = builders[h].builder.finishBlock();
    }
    catch (thinblock_error& e) {
        LogPrintf("%s\n", e.what());
        assert(!"FIXME: Handle finishBlock failing");
    }

    typedef std::set<ThinBlockWorker*>::iterator auto_;
    std::vector<NodeId> peers;
    std::set<ThinBlockWorker*> workers = builders[h].workers;
    for (auto_ w = workers.begin(); w != workers.end(); ++w)
        peers.push_back((*w)->nodeID());

    removeIfExists(h);
    ThinBlockFinishedCallb& callb = *finishedCallb;
    callb(block, peers);
}


ThinBlockWorker::ThinBlockWorker(ThinBlockManager& m, const uint256& block,
        NodeId nodeID) :
    manager(m), block(block), isReRequesting(false), node(nodeID)
{
    if (!block.IsNull())
        manager.addWorker(block, *this);
}

ThinBlockWorker::~ThinBlockWorker() {
    manager.delWorker(*this);
}

bool ThinBlockWorker::addTx(const CTransaction& tx) {
    return manager.addTx(block, tx);
}

std::string ThinBlockWorker::blockStr() const {
    return block.ToString();
}

uint256 ThinBlockWorker::blockHash() const {
    return block;
}

void ThinBlockWorker::setAvailable() {
    manager.delWorker(*this);
    block.SetNull();
    isReRequesting = false;
}

bool ThinBlockWorker::isAvailable() const {
    return block.IsNull();
}

std::vector<uint256> ThinBlockWorker::getTxsMissing() const {
    assert(isStubBuilt());
    return manager.getTxsMissing(block);
}

bool ThinBlockWorker::isStubBuilt() const {
    return manager.isStubBuilt(block);
}

void ThinBlockWorker::setToWork(const uint256& newblock) {
    if (newblock == block)
        return;

    manager.delWorker(*this);
    block = newblock;
    isReRequesting = false;
    manager.addWorker(newblock, *this);
}

void ThinBlockWorker::buildStub(const CMerkleBlock& m, const TxFinder& txFinder) {
    assert(block == m.header.GetHash());
    manager.buildStub(m, txFinder);
}
