// Copyright (c) 2011-2015 The Bitcoin Core developers
// Copyright (c) 2016 The Bitcoin XT developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "blockencodings.h"
#include "chainparams.h"
#include "random.h"
#include "pow.h"
#include "maxblocksize.h"

#include "test/test_bitcoin.h"
#include "test/testutil.h"
#include "test/thinblockutil.h"

#include <boost/test/unit_test.hpp>
#include <stdexcept>
#include <limits>
#include <cmath>

BOOST_AUTO_TEST_SUITE(blockencodings_tests);

BOOST_AUTO_TEST_CASE(TransactionsRequestSerializationTest) {
    CompactReRequest req1;
    req1.blockhash = GetRandHash();
    req1.indexes.resize(4);
    req1.indexes[0] = 0;
    req1.indexes[1] = 1;
    req1.indexes[2] = 3;
    req1.indexes[3] = 4;

    CDataStream stream(SER_NETWORK, PROTOCOL_VERSION);
    stream << req1;

    CompactReRequest req2;
    stream >> req2;

    BOOST_CHECK_EQUAL(req1.blockhash.ToString(), req2.blockhash.ToString());
    BOOST_CHECK_EQUAL(req1.indexes.size(), req2.indexes.size());
    BOOST_CHECK_EQUAL(req1.indexes[0], req2.indexes[0]);
    BOOST_CHECK_EQUAL(req1.indexes[1], req2.indexes[1]);
    BOOST_CHECK_EQUAL(req1.indexes[2], req2.indexes[2]);
    BOOST_CHECK_EQUAL(req1.indexes[3], req2.indexes[3]);
}

BOOST_AUTO_TEST_CASE(validate_compact_block) {
    CBlock block = TestBlock1(); // valid block
    CompactBlock a(block, CoinbaseOnlyPrefiller{});
    const uint64_t currMaxBlockSize = UAHF_INITIAL_MAX_BLOCK_SIZE;
    const uint64_t nextMaxBlockSize = NextBlockRaiseCap(currMaxBlockSize);
    BOOST_CHECK_NO_THROW(validateCompactBlock(a, currMaxBlockSize));

    // invalid header
    CompactBlock b = a;
    b.header.SetNull();
    BOOST_ASSERT(b.header.IsNull());
    BOOST_CHECK_THROW(validateCompactBlock(b, currMaxBlockSize), std::invalid_argument);

    // null tx in prefilled
    CompactBlock c = a;
    c.prefilledtxn.at(0).tx = CTransaction();
    BOOST_CHECK_THROW(validateCompactBlock(c, currMaxBlockSize), std::invalid_argument);

    // overflowing index
    CompactBlock d = a;
    d.prefilledtxn.push_back(d.prefilledtxn[0]);
    assert(d.prefilledtxn.size() == size_t(2));
    d.prefilledtxn.at(0).index = 1;
    d.prefilledtxn.at(1).index = std::numeric_limits<uint32_t>::max();
    BOOST_CHECK_EXCEPTION(validateCompactBlock(d, currMaxBlockSize),
                      std::invalid_argument, errorContains("index overflows"));

    // too high index
    CompactBlock e = a;
    e.prefilledtxn.at(0).index = std::numeric_limits<uint32_t>::max() / 2;
    BOOST_CHECK_EXCEPTION(validateCompactBlock(e, currMaxBlockSize),
                          std::invalid_argument, errorContains("invalid index"));

    // no transactions
    CompactBlock f = a;
    f.shorttxids.clear();
    f.prefilledtxn.clear();
    BOOST_CHECK_THROW(validateCompactBlock(f, currMaxBlockSize), std::invalid_argument);

    // too big
    CompactBlock g = a;
    size_t tooManyTx = std::ceil(nextMaxBlockSize / minTxSize()) + 1;
    g.prefilledtxn.resize(tooManyTx, g.prefilledtxn.at(0));
    BOOST_CHECK_THROW(validateCompactBlock(g, currMaxBlockSize), std::invalid_argument);

    // not too big
    uint64_t nextMaxPrefilled = (nextMaxBlockSize / minTxSize()) - g.shorttxids.size();
    g.prefilledtxn.resize(nextMaxPrefilled);
    BOOST_CHECK_NO_THROW(validateCompactBlock(g, currMaxBlockSize));
}

BOOST_AUTO_TEST_SUITE_END()
