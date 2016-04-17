// Copyright (c) 2015-2016 The Bitcoin XT developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#include "thinblock.h"
#include "thinblockmanager.h"
#include "primitives/block.h"

ThinBlockWorker::ThinBlockWorker(ThinBlockManager& m, NodeId n) :
    mg(m), rerequesting(false), node(n)
{
}

ThinBlockWorker::~ThinBlockWorker() {
    mg.delWorker(*this, node);
}

void ThinBlockWorker::buildStub(const StubData& d, const TxFinder& f) {
    assert(d.header().GetHash() == block);
    mg.buildStub(d, f);
}

bool ThinBlockWorker::isStubBuilt() const {
    return mg.isStubBuilt(block);
}

bool ThinBlockWorker::addTx(const CTransaction& tx) {
    return mg.addTx(block, tx);
}

std::vector<ThinTx> ThinBlockWorker::getTxsMissing() const {
    return mg.getTxsMissing(block);
}

void ThinBlockWorker::setAvailable() {
    if (isAvailable())
        return;
    mg.delWorker(*this, node);
    block.SetNull();
    rerequesting = false;
}

bool ThinBlockWorker::isAvailable() const {
    return block.IsNull();
}

void ThinBlockWorker::setToWork(const uint256& newblock) {
    assert(!newblock.IsNull());
    if (newblock == block)
        return;

    mg.delWorker(*this, node);
    block = newblock;
    rerequesting = false;
    mg.addWorker(newblock, *this);
}

bool ThinBlockWorker::isOnlyWorker() const {
    return mg.numWorkers(block) <= 1;
}

bool ThinBlockWorker::isReRequesting() const {
    return rerequesting;
}

void ThinBlockWorker::setReRequesting(bool r) {
    rerequesting = r;
}

uint256 ThinBlockWorker::blockHash() const {
    return block;
}

std::string ThinBlockWorker::blockStr() const {
    return block.ToString();
}
