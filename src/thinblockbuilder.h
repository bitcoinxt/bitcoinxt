#ifndef BITCOIN_THINBLOCKBUILDER_H
#define BITCOIN_THINBLOCKBUILDER_H

#include "thinblock.h"
#include "primitives/block.h"
#include <boost/shared_ptr.hpp>

class CTransaction;
class CMerkleBlock;
class XThinBlock;

// Assembles a block from it's merkle block and the individual transactions.
class ThinBlockBuilder {
    public:
        ThinBlockBuilder(const CBlockHeader&,
                const std::vector<ThinTx>& txs, const TxFinder&);

        enum TXAddRes {
            TX_ADDED,
            TX_UNWANTED,
            TX_DUP
        };

        TXAddRes addTransaction(const CTransaction& tx);

        int numTxsMissing() const;
        std::vector<std::pair<int, ThinTx> > getTxsMissing() const;

        // If builder has uint256 hashes in wantedTxs list,
        // this will do nothing.
        //
        // If builder has uin64_t hashes, and uint256 are provided,
        // they will be replaced.
        void replaceWantedTx(const std::vector<ThinTx>& tx);

        // Tries to build the block. Throws thinblock_error if it fails.
        // Returns the block (and invalidates this object)
        CBlock finishBlock();

    private:
        CBlock thinBlock;
        std::vector<ThinTx> wantedTxs;
        size_t missing;
};

#endif
