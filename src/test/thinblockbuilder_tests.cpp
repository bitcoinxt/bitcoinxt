// Copyright (c) 2015 The Bitcoin XT developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "thinblockbuilder.h"
#include "primitives/block.h"
#include "bloom.h"
#include "streams.h"
#include "version.h"
#include "serialize.h"
#include "utilstrencodings.h"
#include "merkleblock.h"
#include "test/thinblockutil.h"
#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_SUITE(thinblockbuider_tests);

BOOST_AUTO_TEST_CASE(valid_invalid) {
    ThinBlockBuilder bb;
    BOOST_CHECK(!bb.isValid());

    CBloomFilter emptyFilter;
    CMerkleBlock mblock(TestBlock1(), emptyFilter);
    bb = ThinBlockBuilder(mblock, NullFinder());
    BOOST_CHECK(bb.isValid());
}

BOOST_AUTO_TEST_CASE(uses_txfinder) {
    CBloomFilter emptyFilter;
    CBlock block = TestBlock1();
    CMerkleBlock mblock(block, emptyFilter);
    ThinBlockBuilder bb = ThinBlockBuilder(mblock, NullFinder());
    BOOST_CHECK_EQUAL(9, bb.numTxsMissing());

    struct TwoTxFinder : public TxFinder {
        TwoTxFinder(const CTransaction& a, const CTransaction& b) : A(a), B(b) {
        }

        virtual CTransaction operator()(const uint256& hash) const {
            if (A.GetHash() == hash)
                return A;
            if (B.GetHash() == hash)
                return B;
            return CTransaction();
        }

        CTransaction A, B;
    };

    bb = ThinBlockBuilder(mblock, TwoTxFinder(block.vtx[0], block.vtx[1]));
    BOOST_CHECK_EQUAL(7, bb.numTxsMissing());
}

BOOST_AUTO_TEST_CASE(finish_block) {
    CBloomFilter emptyFilter;
    CBlock block = TestBlock1();
    CMerkleBlock mblock(block, emptyFilter);
    ThinBlockBuilder bb = ThinBlockBuilder(mblock, NullFinder());

    // The order txs are added should not matter.
    std::vector<CTransaction> txs = block.vtx;
    std::random_shuffle(txs.begin(), txs.end());
    typedef std::vector<CTransaction>::const_iterator auto_;
    for (auto_ t = txs.begin(); t != txs.end(); ++t)
        BOOST_CHECK(bb.addTransaction(*t) == ThinBlockBuilder::TX_ADDED);

    CBlock res = bb.finishBlock();
    BOOST_CHECK_EQUAL(block.GetHash().ToString(), res.GetHash().ToString());
}

BOOST_AUTO_TEST_SUITE_END()

