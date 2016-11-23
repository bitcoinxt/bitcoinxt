// Copyright (c) 2016 The Bitcoin XT developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef CompactTxFinderH
#define CompactTxFinderH

#include "uint256.h"
#include "thinblock.h" // TxFinder
#include <unordered_map>

class CTxMemPool;
class CTransaction;
class ThinTx;

// Specialized tx finder for looking up
// compact block transactions in mempool
//
// The generic tx finder is in-efficient for compact blocks
// due to all mempool txs being hashed for every lookup.
class CompactTxFinder : public TxFinder {
    public:

        CompactTxFinder(const CTxMemPool& m,
            uint64_t idk0, uint64_t idk1);

        void initMapping(uint64_t idk0, uint64_t idk1);

        CTransaction operator()(const ThinTx& hash) const override;

    private:
        std::unordered_map<uint64_t, uint256> mappedMempool;
        const CTxMemPool& mempool;
};

#endif
