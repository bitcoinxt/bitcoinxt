// Copyright (c) 2011-2015 The Bitcoin Core developers
// Copyright (c) 2016 The Bitcoin XT developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "blockencodings.h"
#include "chainparams.h"
#include "random.h"
#include "pow.h"

#include "test/test_bitcoin.h"
#include "test/thinblockutil.h"

#include <boost/test/unit_test.hpp>
#include <stdexcept>
#include <limits>

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
    BOOST_CHECK_NO_THROW(validateCompactBlock(a));

    // Invalid header
    CompactBlock b = a;
    b.header.SetNull();
    BOOST_ASSERT(b.header.IsNull());
    BOOST_CHECK_THROW(validateCompactBlock(b), std::invalid_argument);

    // null tx in prefilled
    CompactBlock c = a;
    c.prefilledtxn.at(0).tx = CTransaction();
    BOOST_CHECK_THROW(validateCompactBlock(c), std::invalid_argument);

    // overflowing index
    CompactBlock d = a;
    d.prefilledtxn.at(0).index = std::numeric_limits<uint16_t>::max();
    BOOST_CHECK_THROW(validateCompactBlock(d), std::invalid_argument);

    // too high index
    CompactBlock e = a;
    e.prefilledtxn.at(0).index = std::numeric_limits<uint16_t>::max() / 2;
    BOOST_CHECK_THROW(validateCompactBlock(e), std::invalid_argument);

    // no transactions
    CompactBlock f = a;
    f.shorttxids.clear();
    f.prefilledtxn.clear();
    BOOST_CHECK_THROW(validateCompactBlock(f), std::invalid_argument);
}

BOOST_AUTO_TEST_SUITE_END()
