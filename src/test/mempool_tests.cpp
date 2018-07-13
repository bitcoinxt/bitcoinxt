// Copyright (c) 2011-2014 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "main.h"
#include "txmempool.h"
#include "util.h"

#include "test/test_bitcoin.h"

#include <boost/test/unit_test.hpp>
#include <list>
#include <vector>

BOOST_FIXTURE_TEST_SUITE(mempool_tests, TestingSetup)

BOOST_AUTO_TEST_CASE(MempoolRemoveTest)
{
    // Test CTxMemPool::remove functionality

    TestMemPoolEntryHelper entry;
    // Parent transaction with three children,
    // and three grand-children:
    CMutableTransaction txParent;
    txParent.vin.resize(1);
    txParent.vin[0].scriptSig = CScript() << OP_11;
    txParent.vout.resize(3);
    for (int i = 0; i < 3; i++)
    {
        txParent.vout[i].scriptPubKey = CScript() << OP_11 << OP_EQUAL;
        txParent.vout[i].nValue = 33000LL;
    }
    CMutableTransaction txChild[3];
    for (int i = 0; i < 3; i++)
    {
        txChild[i].vin.resize(1);
        txChild[i].vin[0].scriptSig = CScript() << OP_11;
        txChild[i].vin[0].prevout.hash = txParent.GetHash();
        txChild[i].vin[0].prevout.n = i;
        txChild[i].vout.resize(1);
        txChild[i].vout[0].scriptPubKey = CScript() << OP_11 << OP_EQUAL;
        txChild[i].vout[0].nValue = 11000LL;
    }
    CMutableTransaction txGrandChild[3];
    for (int i = 0; i < 3; i++)
    {
        txGrandChild[i].vin.resize(1);
        txGrandChild[i].vin[0].scriptSig = CScript() << OP_11;
        txGrandChild[i].vin[0].prevout.hash = txChild[i].GetHash();
        txGrandChild[i].vin[0].prevout.n = 0;
        txGrandChild[i].vout.resize(1);
        txGrandChild[i].vout[0].scriptPubKey = CScript() << OP_11 << OP_EQUAL;
        txGrandChild[i].vout[0].nValue = 11000LL;
    }


    CTxMemPool testPool(CFeeRate(0));
    std::list<CTransaction> removed;

    // Nothing in pool, remove should do nothing:
    testPool.removeRecursive(txParent, removed);
    BOOST_CHECK_EQUAL(removed.size(), size_t(0));

    // Just the parent:
    testPool.addUnchecked(txParent.GetHash(), entry.FromTx(txParent));
    testPool.removeRecursive(txParent, removed);
    BOOST_CHECK_EQUAL(removed.size(), size_t(1));
    removed.clear();

    // Parent, children, grandchildren:
    testPool.addUnchecked(txParent.GetHash(), entry.FromTx(txParent));
    for (int i = 0; i < 3; i++)
    {
        testPool.addUnchecked(txChild[i].GetHash(), entry.FromTx(txChild[i]));
        testPool.addUnchecked(txGrandChild[i].GetHash(), entry.FromTx(txGrandChild[i]));
    }
    // Remove Child[0], GrandChild[0] should be removed:
    testPool.removeRecursive(txChild[0], removed);
    BOOST_CHECK_EQUAL(removed.size(), size_t(2));
    removed.clear();
    // ... make sure grandchild and child are gone:
    testPool.removeRecursive(txGrandChild[0], removed);
    BOOST_CHECK_EQUAL(removed.size(), size_t(0));
    testPool.removeRecursive(txChild[0], removed);
    BOOST_CHECK_EQUAL(removed.size(), size_t(0));
    // Remove parent, all children/grandchildren should go:
    testPool.removeRecursive(txParent, removed);
    BOOST_CHECK_EQUAL(removed.size(), size_t(5));
    BOOST_CHECK_EQUAL(testPool.size(), size_t(0));
    removed.clear();

    // Add children and grandchildren, but NOT the parent (simulate the parent being in a block)
    for (int i = 0; i < 3; i++)
    {
        testPool.addUnchecked(txChild[i].GetHash(), entry.FromTx(txChild[i]));
        testPool.addUnchecked(txGrandChild[i].GetHash(), entry.FromTx(txGrandChild[i]));
    }
    // Now remove the parent, as might happen if a block-re-org occurs but the parent cannot be
    // put into the mempool (maybe because it is non-standard):
    testPool.removeRecursive(txParent, removed);
    BOOST_CHECK_EQUAL(removed.size(), size_t(6));
    BOOST_CHECK_EQUAL(testPool.size(), size_t(0));
    removed.clear();
}

template <int indexNumber>
void CheckSort(CTxMemPool &pool, std::vector<std::string> &sortedOrder)
{
    BOOST_CHECK_EQUAL(pool.size(), sortedOrder.size());
    typename CTxMemPool::indexed_transaction_set::nth_index<indexNumber>::type::iterator it = pool.mapTx.get<indexNumber>().begin();
    int count=0;
    for (; it != pool.mapTx.get<indexNumber>().end(); ++it, ++count) {
        BOOST_CHECK_EQUAL(it->GetTx().GetHash().ToString(), sortedOrder[count]);
    }
}

BOOST_AUTO_TEST_CASE(MempoolIndexingTest)
{
    CTxMemPool pool(CFeeRate(0));
    TestMemPoolEntryHelper entry;
    entry.hadNoDependencies = true;

    /* 3rd highest fee */
    CMutableTransaction tx1 = CMutableTransaction();
    tx1.vout.resize(1);
    tx1.vout[0].scriptPubKey = CScript() << OP_11 << OP_EQUAL;
    tx1.vout[0].nValue = 10 * COIN;
    pool.addUnchecked(tx1.GetHash(), entry.Fee(10000LL).FromTx(tx1));

    /* highest fee */
    CMutableTransaction tx2 = CMutableTransaction();
    tx2.vout.resize(1);
    tx2.vout[0].scriptPubKey = CScript() << OP_11 << OP_EQUAL;
    tx2.vout[0].nValue = 2 * COIN;
    pool.addUnchecked(tx2.GetHash(), entry.Fee(20000LL).FromTx(tx2));

    /* lowest fee */
    CMutableTransaction tx3 = CMutableTransaction();
    tx3.vout.resize(1);
    tx3.vout[0].scriptPubKey = CScript() << OP_11 << OP_EQUAL;
    tx3.vout[0].nValue = 5 * COIN;
    pool.addUnchecked(tx3.GetHash(), entry.Fee(0LL).FromTx(tx3));

    /* 2nd highest fee */
    CMutableTransaction tx4 = CMutableTransaction();
    tx4.vout.resize(1);
    tx4.vout[0].scriptPubKey = CScript() << OP_11 << OP_EQUAL;
    tx4.vout[0].nValue = 6 * COIN;
    pool.addUnchecked(tx4.GetHash(), entry.Fee(15000LL).FromTx(tx4));

    /* equal fee rate to tx1, but newer */
    CMutableTransaction tx5 = CMutableTransaction();
    tx5.vout.resize(1);
    tx5.vout[0].scriptPubKey = CScript() << OP_11 << OP_EQUAL;
    tx5.vout[0].nValue = 11 * COIN;
    entry.nTime = 1;
    pool.addUnchecked(tx5.GetHash(), entry.Fee(10000LL).FromTx(tx5));
    BOOST_CHECK_EQUAL(pool.size(), size_t(5));

    std::vector<std::string> sortedOrder;
    sortedOrder.resize(5);
    sortedOrder[0] = tx2.GetHash().ToString(); // 20000
    sortedOrder[1] = tx4.GetHash().ToString(); // 15000
    sortedOrder[2] = tx1.GetHash().ToString(); // 10000
    sortedOrder[3] = tx5.GetHash().ToString(); // 10000
    sortedOrder[4] = tx3.GetHash().ToString(); // 0
    CheckSort<1>(pool, sortedOrder);

    /* low fee but with high fee child */
    /* tx6 -> tx7 -> tx8, tx9 -> tx10 */
    CMutableTransaction tx6 = CMutableTransaction();
    tx6.vout.resize(1);
    tx6.vout[0].scriptPubKey = CScript() << OP_11 << OP_EQUAL;
    tx6.vout[0].nValue = 20 * COIN;
    pool.addUnchecked(tx6.GetHash(), entry.Fee(0LL).FromTx(tx6));
    BOOST_CHECK_EQUAL(pool.size(), size_t(6));
    // Check that at this point, tx6 is sorted low
    sortedOrder.push_back(tx6.GetHash().ToString());
    CheckSort<1>(pool, sortedOrder);

    CTxMemPool::setEntries setAncestors;
    setAncestors.insert(pool.mapTx.find(tx6.GetHash()));
    CMutableTransaction tx7 = CMutableTransaction();
    tx7.vin.resize(1);
    tx7.vin[0].prevout = COutPoint(tx6.GetHash(), 0);
    tx7.vin[0].scriptSig = CScript() << OP_11;
    tx7.vout.resize(2);
    tx7.vout[0].scriptPubKey = CScript() << OP_11 << OP_EQUAL;
    tx7.vout[0].nValue = 10 * COIN;
    tx7.vout[1].scriptPubKey = CScript() << OP_11 << OP_EQUAL;
    tx7.vout[1].nValue = 1 * COIN;

    CTxMemPool::setEntries setAncestorsCalculated;
    std::string dummy;
    BOOST_CHECK_EQUAL(pool.CalculateMemPoolAncestors(entry.Fee(2000000LL).FromTx(tx7), setAncestorsCalculated, 100, 1000000, 1000, 1000000, dummy), true);
    BOOST_CHECK(setAncestorsCalculated == setAncestors);

    pool.addUnchecked(tx7.GetHash(), entry.FromTx(tx7), setAncestors);
    BOOST_CHECK_EQUAL(pool.size(), size_t(7));

    // Now tx6 should be sorted higher (high fee child): tx7, tx6, tx2, ...
    sortedOrder.erase(sortedOrder.end()-1);
    sortedOrder.insert(sortedOrder.begin(), tx6.GetHash().ToString());
    sortedOrder.insert(sortedOrder.begin(), tx7.GetHash().ToString());
    CheckSort<1>(pool, sortedOrder);

    /* low fee child of tx7 */
    CMutableTransaction tx8 = CMutableTransaction();
    tx8.vin.resize(1);
    tx8.vin[0].prevout = COutPoint(tx7.GetHash(), 0);
    tx8.vin[0].scriptSig = CScript() << OP_11;
    tx8.vout.resize(1);
    tx8.vout[0].scriptPubKey = CScript() << OP_11 << OP_EQUAL;
    tx8.vout[0].nValue = 10 * COIN;
    setAncestors.insert(pool.mapTx.find(tx7.GetHash()));
    pool.addUnchecked(tx8.GetHash(), entry.Fee(0LL).Time(2).FromTx(tx8), setAncestors);

    // Now tx8 should be sorted low, but tx6/tx both high
    sortedOrder.push_back(tx8.GetHash().ToString());
    CheckSort<1>(pool, sortedOrder);

    /* low fee child of tx7 */
    CMutableTransaction tx9 = CMutableTransaction();
    tx9.vin.resize(1);
    tx9.vin[0].prevout = COutPoint(tx7.GetHash(), 1);
    tx9.vin[0].scriptSig = CScript() << OP_11;
    tx9.vout.resize(1);
    tx9.vout[0].scriptPubKey = CScript() << OP_11 << OP_EQUAL;
    tx9.vout[0].nValue = 1 * COIN;
    pool.addUnchecked(tx9.GetHash(), entry.Fee(0LL).Time(3).FromTx(tx9), setAncestors);

    // tx9 should be sorted low
    BOOST_CHECK_EQUAL(pool.size(), size_t(9));
    sortedOrder.push_back(tx9.GetHash().ToString());
    CheckSort<1>(pool, sortedOrder);

    std::vector<std::string> snapshotOrder = sortedOrder;

    setAncestors.insert(pool.mapTx.find(tx8.GetHash()));
    setAncestors.insert(pool.mapTx.find(tx9.GetHash()));
    /* tx10 depends on tx8 and tx9 and has a high fee*/
    CMutableTransaction tx10 = CMutableTransaction();
    tx10.vin.resize(2);
    tx10.vin[0].prevout = COutPoint(tx8.GetHash(), 0);
    tx10.vin[0].scriptSig = CScript() << OP_11;
    tx10.vin[1].prevout = COutPoint(tx9.GetHash(), 0);
    tx10.vin[1].scriptSig = CScript() << OP_11;
    tx10.vout.resize(1);
    tx10.vout[0].scriptPubKey = CScript() << OP_11 << OP_EQUAL;
    tx10.vout[0].nValue = 10 * COIN;

    setAncestorsCalculated.clear();
    BOOST_CHECK_EQUAL(pool.CalculateMemPoolAncestors(entry.Fee(200000LL).Time(4).FromTx(tx10), setAncestorsCalculated, 100, 1000000, 1000, 1000000, dummy), true);
    BOOST_CHECK(setAncestorsCalculated == setAncestors);

    pool.addUnchecked(tx10.GetHash(), entry.FromTx(tx10), setAncestors);

    /**
     *  tx8 and tx9 should both now be sorted higher
     *  Final order after tx10 is added:
     *
     *  tx7 = 2.2M (4 txs)
     *  tx6 = 2.2M (5 txs)
     *  tx10 = 200k (1 tx)
     *  tx8 = 200k (2 txs)
     *  tx9 = 200k (2 txs)
     *  tx2 = 20000 (1)
     *  tx4 = 15000 (1)
     *  tx1 = 10000 (1)
     *  tx5 = 10000 (1)
     *  tx3 = 0 (1)
     */
    sortedOrder.erase(sortedOrder.end()-2, sortedOrder.end()); // take out tx8, tx9 from the end
    sortedOrder.insert(sortedOrder.begin()+2, tx10.GetHash().ToString()); // tx10 is after tx6
    sortedOrder.insert(sortedOrder.begin()+3, tx9.GetHash().ToString());
    sortedOrder.insert(sortedOrder.begin()+3, tx8.GetHash().ToString());
    CheckSort<1>(pool, sortedOrder);

    // there should be 10 transactions in the mempool
    BOOST_CHECK_EQUAL(pool.size(), size_t(10));

    // Now try removing tx10 and verify the sort order returns to normal
    std::list<CTransaction> removed;
    pool.removeRecursive(pool.mapTx.find(tx10.GetHash())->GetTx(), removed);
    CheckSort<1>(pool, snapshotOrder);
}

BOOST_AUTO_TEST_CASE(MempoolAncestorIndexingTest)
{
    CTxMemPool pool(CFeeRate(0));

    /* 3rd highest fee */
    CMutableTransaction tx1 = CMutableTransaction();
    tx1.vout.resize(1);
    tx1.vout[0].scriptPubKey = CScript() << OP_11 << OP_EQUAL;
    tx1.vout[0].nValue = 10 * COIN;
    pool.addUnchecked(tx1.GetHash(), CTxMemPoolEntry(tx1, 10000LL, 0, 1, true, false, LockPoints(), 1));

    /* highest fee */
    CMutableTransaction tx2 = CMutableTransaction();
    tx2.vout.resize(1);
    tx2.vout[0].scriptPubKey = CScript() << OP_11 << OP_EQUAL;
    tx2.vout[0].nValue = 2 * COIN;
    pool.addUnchecked(tx2.GetHash(), CTxMemPoolEntry(tx2, 20000LL, 0, 1, true, false, LockPoints(), 1));
    uint64_t tx2Size = ::GetSerializeSize(tx2, SER_NETWORK, PROTOCOL_VERSION);

    /* lowest fee */
    CMutableTransaction tx3 = CMutableTransaction();
    tx3.vout.resize(1);
    tx3.vout[0].scriptPubKey = CScript() << OP_11 << OP_EQUAL;
    tx3.vout[0].nValue = 5 * COIN;
    pool.addUnchecked(tx3.GetHash(), CTxMemPoolEntry(tx3, 0LL, 0, 1, true, false, LockPoints(), 1));

    /* 2nd highest fee */
    CMutableTransaction tx4 = CMutableTransaction();
    tx4.vout.resize(1);
    tx4.vout[0].scriptPubKey = CScript() << OP_11 << OP_EQUAL;
    tx4.vout[0].nValue = 6 * COIN;
    pool.addUnchecked(tx4.GetHash(), CTxMemPoolEntry(tx4, 15000LL, 0, 1, true, false, LockPoints(), 1));

    /* equal fee rate to tx1, but newer */
    CMutableTransaction tx5 = CMutableTransaction();
    tx5.vout.resize(1);
    tx5.vout[0].scriptPubKey = CScript() << OP_11 << OP_EQUAL;
    tx5.vout[0].nValue = 11 * COIN;
    pool.addUnchecked(tx5.GetHash(), CTxMemPoolEntry(tx5, 10000LL, 1, 1, true, false, LockPoints(), 1));
    BOOST_CHECK_EQUAL(pool.size(), size_t(5));

    std::vector<std::string> sortedOrder;
    sortedOrder.resize(5);
    sortedOrder[0] = tx2.GetHash().ToString(); // 20000
    sortedOrder[1] = tx4.GetHash().ToString(); // 15000
    sortedOrder[2] = tx1.GetHash().ToString(); // 10000
    sortedOrder[3] = tx5.GetHash().ToString(); // 10000
    sortedOrder[4] = tx3.GetHash().ToString(); // 0
    CheckSort<3>(pool, sortedOrder);

    /* low fee parent with high fee child */
    /* tx6 (0) -> tx7 (high) */
    CMutableTransaction tx6 = CMutableTransaction();
    tx6.vout.resize(1);
    tx6.vout[0].scriptPubKey = CScript() << OP_11 << OP_EQUAL;
    tx6.vout[0].nValue = 20 * COIN;
    uint64_t tx6Size = ::GetSerializeSize(tx6, SER_NETWORK, PROTOCOL_VERSION);

    pool.addUnchecked(tx6.GetHash(), CTxMemPoolEntry(tx6, 0LL, 1, 1, true, false, LockPoints(), 1));
    BOOST_CHECK_EQUAL(pool.size(), size_t(6));
    sortedOrder.push_back(tx6.GetHash().ToString());
    CheckSort<3>(pool, sortedOrder);

    CMutableTransaction tx7 = CMutableTransaction();
    tx7.vin.resize(1);
    tx7.vin[0].prevout = COutPoint(tx6.GetHash(), 0);
    tx7.vin[0].scriptSig = CScript() << OP_11;
    tx7.vout.resize(1);
    tx7.vout[0].scriptPubKey = CScript() << OP_11 << OP_EQUAL;
    tx7.vout[0].nValue = 10 * COIN;
    uint64_t tx7Size = ::GetSerializeSize(tx7, SER_NETWORK, PROTOCOL_VERSION);

    /* set the fee to just below tx2's feerate when including ancestor */
    CAmount fee = (20000/tx2Size)*(tx7Size + tx6Size) - 1;

    CTxMemPoolEntry entry7(tx7, fee, 2, 1, false, false, LockPoints(), 1);
    pool.addUnchecked(tx7.GetHash(), entry7);
    BOOST_CHECK_EQUAL(pool.size(), size_t(7));
    sortedOrder.insert(sortedOrder.begin()+1, tx7.GetHash().ToString());
    CheckSort<3>(pool, sortedOrder);

    /* after tx6 is mined, tx7 should move up in the sort */
    std::vector<CTransaction> vtx;
    vtx.push_back(tx6);
    std::list<CTransaction> dummy;
    pool.removeForBlock(vtx, 1, dummy, false);

    sortedOrder.erase(sortedOrder.begin()+1);
    sortedOrder.pop_back();
    sortedOrder.insert(sortedOrder.begin(), tx7.GetHash().ToString());
    CheckSort<3>(pool, sortedOrder);
}

BOOST_AUTO_TEST_SUITE_END()
