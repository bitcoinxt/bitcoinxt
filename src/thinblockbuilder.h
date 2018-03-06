#ifndef BITCOIN_THINBLOCKBUILDER_H
#define BITCOIN_THINBLOCKBUILDER_H

#include "thinblock.h"
#include "primitives/block.h"
#include "random.h"

#include <unordered_set>
#include <unordered_map>
#include <vector>

class CTransaction;
class CMerkleBlock;
class XThinBlock;

class IdkHasher {
    public:
        IdkHasher() : nonce(GetRand(std::numeric_limits<uint64_t>::max())) { }
        size_t operator()(const std::pair<uint64_t, uint64_t>& h) const {
            return h.first ^ h.second ^ nonce;
        }

    private:
        uint64_t nonce;
};

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
        std::vector<ThinTx> wanted;
        std::unordered_set<std::pair<uint64_t, uint64_t>, IdkHasher> wantedIdks;
        std::unordered_map<uint64_t, std::vector<ThinTx>::iterator> wantedIndex;
        size_t missing;

        void updateWantedIndex();
};

#endif
