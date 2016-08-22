// Copyright (c) 2016- The Bitcoin XT developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "compactthin.h"
#include "protocol.h"
#include <sstream>

CompactWorker::CompactWorker(ThinBlockManager& m, NodeId n) :
    ThinBlockWorker(m, n)
{
}

void CompactWorker::requestBlock(const uint256& block,
        std::vector<CInv>& getDataReq, CNode& node) {

    getDataReq.push_back(CInv(MSG_CMPCT_BLOCK, block));
}

std::vector<ThinTx> CompactStub::allTransactions() const {

    std::vector<ThinTx> all(block.BlockTxCount(), ThinTx(uint256()));

    int lastIndex = -1;
    for (size_t i = 0; i < block.prefilledtxn.size(); ++i) {
        lastIndex += block.prefilledtxn.at(i).index + 1;
        all.at(lastIndex) = ThinTx(
            block.prefilledtxn.at(i).tx.GetHash());

    }

    uint64_t offset = 0;
    for (size_t i = 0; i < block.shorttxids.size(); ++i) {
        while (all.at(i + offset).hasFull())
            ++offset;

        all.at(i + offset) = ThinTx(block.shorttxids.at(i),
                    block.shorttxidk0, block.shorttxidk1);
    }

    for (size_t i = 0; i < all.size(); ++i) {
        if (!all[i].isNull())
            continue;
        std::stringstream err;
        err << "Failed to read all tx ids from block. "
            << "Tx at index " << i << " is null "
            << " (total " <<  all.size() << "txs)";
        throw thinblock_error(err.str());
    }
    return all;
}
