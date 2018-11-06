// Copyright (c) 2018 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <boost/test/unit_test.hpp>

#include "utilblock.h"
#include "primitives/transaction.h"

BOOST_AUTO_TEST_SUITE(utilblock_tests);

BOOST_AUTO_TEST_CASE(sort_by_parents_first) {
    std::vector<CTransaction> txs;

    CMutableTransaction a;
    a.nVersion = 42;
    txs.push_back(a);

    CMutableTransaction b;
    b.vin.resize(1);
    b.vin[0].prevout.hash = txs[0].GetHash();
    txs.push_back(b);

    CMutableTransaction c;
    c.vin.resize(1);
    c.vin[0].prevout.hash = txs[1].GetHash();
    txs.push_back(c);


    // Send in reverse order. The sort function should sort it back to
    // parent-first.
    std::vector<CTransaction> wrong_order(txs.rbegin(), txs.rend());
    std::vector<CTransaction> result
        = SortByParentsFirst(begin(wrong_order), end(wrong_order));

    BOOST_CHECK(result.size() == txs.size() && result.size() == size_t(3));
    for (size_t i = 0; i < 3; ++i) {
        BOOST_CHECK(result[i] == txs[i]);
    }
}

BOOST_AUTO_TEST_SUITE_END();
