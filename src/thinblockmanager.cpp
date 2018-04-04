// Copyright (c) 2015-2016 The Bitcoin XT developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#include "thinblockmanager.h"
#include "thinblock.h"
#include "thinblockbuilder.h"
#include "util.h"
#include <algorithm>

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

void ThinBlockManager::delWorker(const uint256& block, ThinBlockWorker& w) {
    assert(builders.count(block));

    bool ok = builders[block].workers.erase(&w);
    assert(ok); // worker was working on block.

    if (builders[block].workers.empty())
        builders.erase(block);

    InFlightEraser& erase = *inFlightEraser;
    erase(w.nodeID(), block);
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

void ThinBlockManager::buildStub(const StubData& s, const TxFinder& txFinder,
        ThinBlockWorker& worker, CNode& from)
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

        // Node was first to provide us
        // a thin block. Select for block announcements with thin blocks.
        requestBlockAnnouncements(worker, from);
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
        LogPrint(Log::BLOCK, "tx %s does not belong to block %s\n",
                tx.GetHash().ToString(), block.ToString());
        return false;
    }

    else if (res == ThinBlockBuilder::TX_DUP) { /* ignore */ }

    else if (res == ThinBlockBuilder::TX_ADDED) { /* ignore */ }

    else { assert(!"unknown addTransaction response"); }

    if (b->numTxsMissing() == 0)
        finishBlock(block, *b);

    return true;
}

void ThinBlockManager::removeIfExists(const uint256& h) {
    if (!builders.count(h))
        return;

    // Take copy. Calling setAvailable causes workers to remove
    // themself from set (changing it during iteration)
    std::set<ThinBlockWorker*> workers = builders[h].workers;
    for (ThinBlockWorker* w : workers)
        w->stopWork(h);
    builders.erase(h);
}

std::vector<std::pair<int, ThinTx> > ThinBlockManager::getTxsMissing(const uint256& hash) const {
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

// We ask for announcements with blocks from the
// last 3 peers to provide a thin block first.
void ThinBlockManager::requestBlockAnnouncements(ThinBlockWorker& w, CNode& node) {

    typedef std::unique_ptr<BlockAnnHandle> annh;
    auto it = std::find_if(
            begin(announcers), end(announcers), [&w](const annh& h) {
        return w.nodeID() == h->nodeID();
    });

    if (it != end(announcers)) {
        // Already receiving announcements from peer.
        // Move to the front.
        std::rotate(it, it + 1, end(announcers));
        return;
    }

    auto handle = w.requestBlockAnnouncements(node);
    if (!bool(handle)) {
        // Peer cannot provide block announcements
        // with thin blocks.
        return;
    }

    announcers.push_back(std::move(handle));

    // Only request thin block announcements from 3 peers at a time.
    if (announcers.size() > 3)
        announcers.erase(begin(announcers));
    LogPrint(Log::ANN, "Thin block announcers: %d\n", announcers.size());
}
