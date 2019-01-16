// Copyright (c) 2018 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

// Tests for CUtxoCommit wrapper.
// Mostly redundant with libsecp256k1_multiset tests

#include "coins.h"
#include "test/test_bitcoin.h"
#include "test/testutil.h"
#include "util.h"
#include "random.h"

#include <vector>

#include <boost/test/unit_test.hpp>

#include "secp256k1/include/secp256k1_multiset.h"
#include "utxocommit.h"
#include "utilutxocommit.h"

static COutPoint RandomOutpoint() {
    const COutPoint op(GetRandHash(), (uint32_t)GetRandInt(1000));
    return op;
}

static Coin RandomCoin() {
    const int scriptlen = GetRandInt(0x3f);
    auto script = std::vector<unsigned char>(scriptlen);
    GetRandBytes(&script[0], scriptlen);
    const Coin c(CTxOut(CAmount(GetRandInt(1000)), CScript(script)),
                 GetRandInt(10000), GetRandInt(1));
    return c;
}

BOOST_FIXTURE_TEST_SUITE(utxocommit_tests, TestingSetup)

BOOST_AUTO_TEST_CASE(utxo_commit_order) {

    // Test order independence

    const COutPoint op1 = RandomOutpoint();
    const COutPoint op2 = RandomOutpoint();
    const COutPoint op3 = RandomOutpoint();
    const Coin c1 = RandomCoin();
    const Coin c2 = RandomCoin();
    const Coin c3 = RandomCoin();

    CUtxoCommit uc1, uc2, uc3;
    BOOST_CHECK(uc1 == uc2);
    uc1.Add(op1, c1);
    uc1.Add(op2, c2);
    uc1.Add(op3, c3);

    uc2.Add(op2, c2);
    BOOST_CHECK(uc1 != uc2);
    uc2.Add(op3, c3);
    uc2.Add(op1, c1);
    BOOST_CHECK(uc1 == uc2);

    // remove ordering
    uc2.Remove(op2, c2);
    uc2.Remove(op3, c3);

    uc1.Remove(op3, c3);
    uc1.Remove(op2, c2);

    BOOST_CHECK(uc1 == uc2);

    // odd but allowed
    uc3.Remove(op2, c2);
    uc3.Add(op2, c2);
    uc3.Add(op1, c1);
    BOOST_CHECK(uc1 == uc3);
}

BOOST_AUTO_TEST_CASE(utxo_commit_serialize) {

    // Test whether the serialization is as expected

    // some coin & output
    const std::vector<uint8_t> txid = ParseHex(
        "38115d014104c6ec27cffce0823c3fecb162dbd576c88dd7cda0b7b32b096118");
    const uint32_t output = 2;
    const uint32_t height = 7;
    const uint64_t amount = 100;

    const auto script =
        CScript(ParseHex("76A9148ABCDEFABBAABBAABBAABBAABBAABBAABBAABBA88AC"));

    const COutPoint op(uint256(txid), output);
    const Coin coin = Coin(CTxOut(CAmount(amount), script), height, false);
    CScript s;

    // find commit
    CUtxoCommit commit;
    commit.Add(op, coin);
    uint256 hash = commit.GetHash();

    // try the same manually
    std::vector<uint8_t> expected;

    // txid
    expected.insert(expected.end(), txid.begin(), txid.end());

    // output
    auto outputbytes = ParseHex("02000000");
    expected.insert(expected.end(), outputbytes.begin(), outputbytes.end());

    // height and coinbase => height * 2
    expected.push_back(14);

    // amount & script
    auto amountbytes = ParseHex("6400000000000000");
    expected.insert(expected.end(), amountbytes.begin(), amountbytes.end());
    expected.push_back(uint8_t(script.size()));
    expected.insert(expected.end(), script.begin(), script.end());

    secp256k1_context *ctx;
    ctx = secp256k1_context_create(SECP256K1_CONTEXT_NONE);
    secp256k1_multiset multiset;
    secp256k1_multiset_init(ctx, &multiset);
    secp256k1_multiset_add(ctx, &multiset, expected.data(), expected.size());

    std::vector<uint8_t> expectedhash(32);
    secp256k1_multiset_finalize(ctx, expectedhash.data(), &multiset);

    secp256k1_context_destroy(ctx);
    BOOST_ASSERT(uint256(expectedhash) == hash);
}

BOOST_AUTO_TEST_CASE(utxo_commit_addcursor) {

    // Test adding a CCoinView Cursor to a CUtxoCommit
    // This simulates the initial upgrade where the commitment stored in
    // LevelDB must be generated from the existing UTXO set.

    const int count = 50000;

    // We use the pcoinviewdb provided by the test fixture's TestingSetup
    CCoinsViewCache cache(pcoinsdbview);
    cache.SetBestBlock(GetRandHash());

    // We will compare the commitment generated step-by-step, and the one
    // created
    // from cursor
    CUtxoCommit commit_step;

    LogPrintf("Preparing database\n");

    for (int n = 0; n < count; n++) {
        const COutPoint op = RandomOutpoint();
        Coin c = RandomCoin();

        if (c.out.scriptPubKey.IsUnspendable()) {
            continue;
        }

        commit_step.Add(op, c);
        cache.AddCoin(op, std::move(c), false);
        if ((n + 1) % 5000000 == 0) {
            LogPrintf("Flushing\n");
            BOOST_ASSERT(cache.Flush());
        }
    }

    BOOST_ASSERT(cache.Flush());
    LogPrintf("Starting ECMH generation from cursor\n");

    std::unique_ptr<CCoinsViewCursor> pcursor(pcoinsdbview->Cursor());
    CUtxoCommit commit_cursor = BuildUtxoCommit(pcursor.get(), 10);

    BOOST_CHECK(commit_step == commit_cursor);
    LogPrintf("ECMH generation from cursor done\n");
}

BOOST_AUTO_TEST_SUITE_END()
