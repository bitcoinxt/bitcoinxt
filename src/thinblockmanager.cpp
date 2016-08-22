// Copyright (c) 2015-2016 The Bitcoin XT developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#include "thinblockmanager.h"
#include "thinblock.h"
#include "thinblockbuilder.h"
#include "util.h"

ThinBlockManager::ThinBlockManager(
        std::unique_ptr<ThinBlockFinishedCallb> callb,
        std::unique_ptr<InFlightEraser> inFlightEraser) :
    finishedCallb(callb.release()), inFlightEraser(inFlightEraser.release())
{ }

void ThinBlockManager::addWorker(const uint256& block, ThinBlockWorker& w) {
    if (!builders.count(block))
        builders[block] = ThinBlockManager::ActiveBuilder();
    builders[block].workers.insert(&w);
}

void ThinBlockManager::delWorker(ThinBlockWorker& w, NodeId nodeId) {
    typedef std::map<uint256, ActiveBuilder>::iterator auto_;
    for (auto_ a = builders.begin(); a != builders.end(); ++a) {
        if (!a->second.workers.erase(&w))
            continue; // not working on block.

        if (a->second.workers.empty())
            builders.erase(a);

        InFlightEraser& erase = *inFlightEraser;
        erase(nodeId, a->first);

        // worker only works on one thin block at
        // a time, so safe to return.
        return;
    }
}
int ThinBlockManager::numWorkers(const uint256& block) const {
    return builders.count(block)
        ? builders.find(block)->second.workers.size()
        : 0;
}

// When a stub provides transactions, add those to the transaction finder (wrap it).
struct WrappedFinder : public TxFinder {

    WrappedFinder(const std::vector<CTransaction>& stubTxs, const TxFinder& w) :
        txs(stubTxs), wrapped(w)
    { }

    virtual CTransaction operator()(const ThinTx& hash) const {

        for (const CTransaction& t : txs)
            if (hash.equals(t.GetHash()))
                return t;

        return wrapped(hash);
    }

    const std::vector<CTransaction> txs;
    const TxFinder& wrapped;
};

void ThinBlockManager::buildStub(const StubData& s, const TxFinder& txFinder)
{
    uint256 h = s.header().GetHash();
    assert(builders.count(h));
    if (builders[h].builder)
    {
        try {
            builders[h].builder->replaceWantedTx(s.allTransactions());
        }
        catch (const thinblock_error& e) {
            LogPrintf("Error: Thinblock stub mismatch: %s. Giving up on downloading block %s\n",
                    e.what(), h.ToString());
            std::cerr << "thinblock error" << e.what() << std::endl;

            // FIXME: If this actually happens, we should mark
            // all workers working on block as misbehaving. We don't know
            // who provided a bad stub.
            removeIfExists(h);
            return;
        }

        // The stub may provide transactions we're missing.
        std::vector<CTransaction> provided = s.missingProvided();
        for (auto& tx : provided)
            builders[h].builder->addTransaction(tx);
    }
    else {
        WrappedFinder wfinder(s.missingProvided(), txFinder);
        builders[h].builder.reset(new ThinBlockBuilder(
            s.header(), s.allTransactions(), wfinder));
    }

    if (builders[h].builder->numTxsMissing() == 0)
        finishBlock(h, *builders[h].builder);
}

bool ThinBlockManager::isStubBuilt(const uint256& block) {
    if (!builders.count(block))
        return false;

    ActiveBuilder& b = builders[block];
    return bool(b.builder);
}

// Try to add transaction to the block peer is providing.
// Returns true if tx belonged to block
bool ThinBlockManager::addTx(const uint256& block, const CTransaction& tx) {

    if (!builders.count(block))
        return false;

    ThinBlockBuilder* b = builders[block].builder.get();
    if (!b)
        return false;

    ThinBlockBuilder::TXAddRes res = b->addTransaction(tx);

    if (res == ThinBlockBuilder::TX_UNWANTED) {
        LogPrint("thin", "tx %s does not belong to block %s\n",
                tx.GetHash().ToString(), block.ToString());
        return false;
    }

    else if (res == ThinBlockBuilder::TX_DUP)
        LogPrint("thin2", "already had tx %s\n",
                tx.GetHash().ToString());

    else if (res == ThinBlockBuilder::TX_ADDED)
        LogPrint("thin2", "added transaction %s\n",
                tx.GetHash().ToString());

    else { assert(!"unknown addTransaction response"); }

    if (b->numTxsMissing() == 0)
        finishBlock(block, *b);

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

std::vector<ThinTx> ThinBlockManager::getTxsMissing(const uint256& hash) const {
    assert(builders.count(hash));
    ThinBlockBuilder* b = builders.find(hash)->second.builder.get();
    assert(b);
    return b->getTxsMissing();
}

void ThinBlockManager::finishBlock(const uint256& h, ThinBlockBuilder& builder) {
    CBlock block;
    try {
        block = builder.finishBlock();
    }
    catch (thinblock_error& e) {
        LogPrintf("ERROR: a thinblock builder failed to finish block %s - %s\n",
                block.ToString(), e.what());

        // Give up on block
        removeIfExists(h);
        return;
    }

    typedef std::set<ThinBlockWorker*>::iterator auto_;
    std::vector<NodeId> peers;
    std::set<ThinBlockWorker*> workers = builders[h].workers;
    for (auto_ w = workers.begin(); w != workers.end(); ++w)
        peers.push_back((*w)->nodeID());

    ThinBlockFinishedCallb& callb = *finishedCallb;
    callb(block, peers);
    removeIfExists(h);
}

