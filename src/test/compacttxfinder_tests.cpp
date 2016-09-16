// Copyright (c) 2016 The Bitcoin XT developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#include <boost/test/unit_test_suite.hpp>
#include <boost/test/test_tools.hpp>
#include "blockencodings.h"
#include "compactprefiller.h"
#include "compactthin.h"
#include "compacttxfinder.h"
#include "sync.h"
#include "test/test_bitcoin.h"
#include "test/thinblockutil.h"
#include "txmempool.h"

BOOST_AUTO_TEST_SUITE(comapacttxfinder_tests)

BOOST_AUTO_TEST_CASE(compacttxfinder_test) {

    CTxMemPool mpool(CFeeRate(0));
    TestMemPoolEntryHelper entry;

    CBlock block = TestBlock1();
    mpool.addUnchecked(block.vtx[1].GetHash(), entry.FromTx(block.vtx[1]));
    mpool.addUnchecked(block.vtx[2].GetHash(), entry.FromTx(block.vtx[2]));
    mpool.addUnchecked(block.vtx[3].GetHash(), entry.FromTx(block.vtx[3]));

    CompactBlock cmpct(block, CoinbaseOnlyPrefiller());
    CompactStub stub(cmpct);

    std::vector<ThinTx> all = stub.allTransactions();

    CompactTxFinder finder(mpool, cmpct.shorttxidk0, cmpct.shorttxidk1);

    // Should find the txs in mpool
    BOOST_CHECK_EQUAL(
            block.vtx[1].GetHash().ToString(),
            finder(all[1]).GetHash().ToString());
    BOOST_CHECK_EQUAL(
            block.vtx[2].GetHash().ToString(),
            finder(all[2]).GetHash().ToString());
    BOOST_CHECK_EQUAL(
            block.vtx[3].GetHash().ToString(),
            finder(all[3]).GetHash().ToString());

    // Should not find txs not in mempool
    BOOST_CHECK(finder(all[4]).IsNull());

    // If tx is erased from mempool, it should not be found
    LOCK(mpool.cs);
    auto i = mpool.mapTx.find(block.vtx[1].GetHash());
    mpool.mapTx.erase(i);
    BOOST_CHECK(finder(all[1]).IsNull());
}


BOOST_AUTO_TEST_SUITE_END()
