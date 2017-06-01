// Copyright (c) 2015 The Bitcoin XT developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "thinblockbuilder.h"
#include "bloom.h"
#include "merkleblock.h"
#include "primitives/block.h"
#include "serialize.h"
#include "streams.h"
#include "test/thinblockutil.h"
#include "utilstrencodings.h"
#include "version.h"
#include "xthin.h"
#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_SUITE(thinblockbuider_tests);

BOOST_AUTO_TEST_CASE(uses_txfinder) {
    CBlock block = TestBlock1();
    XThinBlock xblock(block, CBloomFilter());
    XThinStub stub(xblock);
    ThinBlockBuilder bb = ThinBlockBuilder(stub.header(),
            stub.allTransactions(), NullFinder());
    BOOST_CHECK_EQUAL(9, bb.numTxsMissing());

    struct TwoTxFinder : public TxFinder {
        TwoTxFinder(const CTransaction& a, const CTransaction& b) : A(a), B(b) {
        }

        virtual CTransaction operator()(const ThinTx& hash) const {
            if (hash.equals(ThinTx(A.GetHash())))
                return A;
            if (hash.equals(ThinTx(B.GetHash())))
                return B;
            return CTransaction();
        }

        CTransaction A, B;
    };

    bb = ThinBlockBuilder(stub.header(), stub.allTransactions(), TwoTxFinder(block.vtx[0], block.vtx[1]));
    BOOST_CHECK_EQUAL(7, bb.numTxsMissing());
}

BOOST_AUTO_TEST_CASE(finish_block) {
    CBloomFilter emptyFilter;
    CBlock block = TestBlock1();
    XThinStub stub(XThinBlock(block, emptyFilter));
    ThinBlockBuilder bb(stub.header(), stub.allTransactions(), NullFinder());

    // The order txs are added should not matter.
    std::vector<CTransaction> txs = block.vtx;
    std::random_shuffle(txs.begin(), txs.end());
    for (auto& t : txs)
        BOOST_CHECK(bb.addTransaction(t) == ThinBlockBuilder::TX_ADDED);

    CBlock res = bb.finishBlock();
    BOOST_CHECK_EQUAL(block.GetHash().ToString(), res.GetHash().ToString());
}

BOOST_AUTO_TEST_SUITE_END()

