// Copyright (c) 2015-2018 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#include "thinblock.h"
#include "thinblockmanager.h"
#include "primitives/block.h"
#include "blockencodings.h" // GetShortID
#include "util.h"
#include <sstream>


ThinTx::ThinTx(const uint256& full) : full_(full), hasFull_(true)
{
    shortid_.id = 0;
}

ThinTx::ThinTx(const uint64_t& cheap) : cheap_(cheap)
{
    shortid_.id = 0;
}

ThinTx::ThinTx(const uint64_t& shortid,
               const std::pair<uint64_t, uint64_t>& idk)
{
    shortid_.id = shortid;
    shortid_.idk = idk;
}

ThinTx::ThinTx(const uint256& full, const std::pair<uint64_t, uint64_t>& idk)
    : full_(full), hasFull_(true)
{
    shortid_.idk = idk;
    shortid_.id = GetShortID(idk.first, idk.second, full);
}

void ThinTx::merge(const ThinTx& tx) {
    if (hasFull())
        return;

    if (tx.hasFull()) {
        full_ = tx.full();
        return;
    }

    if (!hasCheap() && tx.hasCheap())
        cheap_ = tx.cheap();

    if (hasShortid())
        return;

    if (!tx.hasShortid())
        return;

    shortid_ = tx.shortid_;
}

uint64_t ThinTx::cheap() const {
    if (cheap_ == 0 && hasFull())
        cheap_ = full_.GetCheapHash();

    return cheap_;
}

// Returns false if transactions don't match OR if it's indeterminate.
bool ThinTx::equals(const ThinTx& b) const {
    const ThinTx& a = *this;
    const bool a_hasShortid = a.hasShortid();
    const bool b_hasShortid = b.hasShortid();

    if (a_hasShortid && b_hasShortid) {
        if (a.shortid_.idk != b.shortid_.idk)
        {
            // different salt, indeterminate, continue to try other comparisons.
        }
        else {
            return shortid_.id == b.shortid_.id;
        }
    }

    const bool a_hasFull = a.hasFull();
    if (a_hasFull && b_hasShortid)
        return b.shortid_.id == GetShortID(b.shortid_.idk, a.full_);

    const bool b_hasFull = b.hasFull();
    if (a_hasShortid && b_hasFull)
        return a.shortid_.id == GetShortID(a.shortid_.idk, b.full_);

    if (a_hasFull && b_hasFull)
        return a.full_ == b.full_;

    const bool a_hasCheap = a_hasFull || a.hasCheap();
    const bool b_hasCheap = b_hasFull || b.hasCheap();

    if (a_hasCheap && b_hasCheap)
        return cheap() == b.cheap();

    // same as a.isNull() && b.isNull()
    return !(a_hasFull | b_hasFull | a_hasCheap | b_hasCheap | a_hasShortid | b_hasShortid);
}

ThinBlockWorker::ThinBlockWorker(ThinBlockManager& m, NodeId n) :
    mg(m), node(n)
{
}

ThinBlockWorker::~ThinBlockWorker() {
    stopAllWork();
}

void ThinBlockWorker::buildStub(const StubData& d, const TxFinder& f,
                                CConnman& connman, CNode& from)
{
    assert(isWorkingOn(d.header().GetHash()));
    return mg.buildStub(d, f, *this, connman, from);
}

bool ThinBlockWorker::isStubBuilt(const uint256& block) const {
    return mg.isStubBuilt(block);
}

bool ThinBlockWorker::addTx(const uint256& block, const CTransaction& tx) {
    return mg.addTx(block, tx);
}

std::vector<std::pair<int, ThinTx> > ThinBlockWorker::getTxsMissing(const uint256& block) const {
    return mg.getTxsMissing(block);
}

void ThinBlockWorker::stopWork(const uint256& block) {
    if (!isWorkingOn(block))
        return;

    mg.delWorker(block, *this);
    blocks.erase(block);
    rerequesting.erase(block);
}

void ThinBlockWorker::stopAllWork() {
    std::set<uint256> blocksCpy = blocks;
    for (const uint256& b : blocksCpy)
        stopWork(b);
}

void ThinBlockWorker::addWork(const uint256& newblock) {
    assert(!newblock.IsNull());
    if (isWorkingOn(newblock))
        return;
    blocks.insert(newblock);
    mg.addWorker(newblock, *this);
}

bool ThinBlockWorker::isOnlyWorker(const uint256& block) const {
    return mg.numWorkers(block) <= 1;
}

bool ThinBlockWorker::isReRequesting(const uint256& block) const {
    return rerequesting.count(block);
}

void ThinBlockWorker::setReRequesting(const uint256& block, bool r) {
    if (r)
        rerequesting.insert(block);
    else
        rerequesting.erase(block);
}
