// Copyright (c) 2012-2013 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "consensus/consensus.h"
#include "consensus/tx_verify.h"
#include "consensus/validation.h"
#include "random.h"
#include "policy/policy.h" // For STANDARD_CHECKDATASIG_VERIFY_FLAGS.
#include "pubkey.h"
#include "key.h"
#include "script/script.h"
#include "script/standard.h"
#include "uint256.h"
#include "test/test_bitcoin.h"

#include <vector>

#include <boost/foreach.hpp>
#include <boost/test/unit_test.hpp>

using namespace std;

// Helpers:
static std::vector<unsigned char>
Serialize(const CScript& s)
{
    std::vector<unsigned char> sSerialized(s);
    return sSerialized;
}

BOOST_FIXTURE_TEST_SUITE(sigopcount_tests, BasicTestingSetup)

void CheckScriptSigOps(const CScript &script, uint32_t accurate_sigops,
                       uint32_t inaccurate_sigops, uint32_t datasigops) {
    const uint32_t stdflags = STANDARD_SCRIPT_VERIFY_FLAGS;
    const uint32_t datasigflags = STANDARD_CHECKDATASIG_VERIFY_FLAGS;

    BOOST_CHECK_EQUAL(script.GetSigOpCount(stdflags, false), inaccurate_sigops);
    BOOST_CHECK_EQUAL(script.GetSigOpCount(datasigflags, false),
                      inaccurate_sigops + datasigops);
    BOOST_CHECK_EQUAL(script.GetSigOpCount(stdflags, true), accurate_sigops);
    BOOST_CHECK_EQUAL(script.GetSigOpCount(datasigflags, true),
                      accurate_sigops + datasigops);

    const CScript p2sh = GetScriptForDestination(CScriptID(script));
    const CScript scriptSig = CScript() << OP_0 << Serialize(script);
    BOOST_CHECK_EQUAL(p2sh.GetSigOpCount(stdflags, scriptSig), accurate_sigops);
    BOOST_CHECK_EQUAL(p2sh.GetSigOpCount(datasigflags, scriptSig),
                      accurate_sigops + datasigops);

    // Check that GetSigOpCount do not report sigops in the P2SH script when the
    // P2SH flags isn't passed in.
    BOOST_CHECK_EQUAL(p2sh.GetSigOpCount(SCRIPT_VERIFY_NONE, scriptSig), 0U);

    // Check that GetSigOpCount report the exact count when not passed a P2SH.
    BOOST_CHECK_EQUAL(script.GetSigOpCount(stdflags, p2sh), accurate_sigops);
    BOOST_CHECK_EQUAL(script.GetSigOpCount(datasigflags, p2sh),
                      accurate_sigops + datasigops);
    BOOST_CHECK_EQUAL(script.GetSigOpCount(SCRIPT_VERIFY_NONE, p2sh),
                      accurate_sigops);
}

BOOST_AUTO_TEST_CASE(GetSigOpCount) {
    // Test CScript::GetSigOpCount()
    CheckScriptSigOps(CScript(), 0, 0, 0);

    uint160 dummy;
    const CScript s1 = CScript()
                       << OP_1 << ToByteVector(dummy) << ToByteVector(dummy)
                       << OP_2 << OP_CHECKMULTISIG;
    CheckScriptSigOps(s1, 2, 20, 0);

    const CScript s2 = CScript(s1) << OP_IF << OP_CHECKSIG << OP_ENDIF;
    CheckScriptSigOps(s2, 3, 21, 0);

    std::vector<CPubKey> keys;
    for (int i = 0; i < 3; i++)
    {
        CKey k;
        k.MakeNewKey(true);
        keys.push_back(k.GetPubKey());
    }

    const CScript s3 = GetScriptForMultisig(1, keys);
    CheckScriptSigOps(s3, 3, 20, 0);

    const CScript p2sh = GetScriptForDestination(CScriptID(s3));
    CheckScriptSigOps(p2sh, 0, 0, 0);

    CScript scriptSig2;
    scriptSig2 << OP_1 << ToByteVector(dummy) << ToByteVector(dummy)
               << Serialize(s3);
    BOOST_CHECK_EQUAL(
        p2sh.GetSigOpCount(STANDARD_SCRIPT_VERIFY_FLAGS, scriptSig2), 3U);
    BOOST_CHECK_EQUAL(
        p2sh.GetSigOpCount(STANDARD_CHECKDATASIG_VERIFY_FLAGS, scriptSig2), 3U);
    BOOST_CHECK_EQUAL(p2sh.GetSigOpCount(SCRIPT_VERIFY_NONE, scriptSig2), 0U);

    const CScript s4 = CScript(s1) << OP_IF << OP_CHECKDATASIG << OP_ENDIF;
    CheckScriptSigOps(s4, 2, 20, 1);

    const CScript s5 = CScript(s4) << OP_CHECKDATASIGVERIFY;
    CheckScriptSigOps(s5, 2, 20, 2);
}

BOOST_AUTO_TEST_CASE(test_max_sigops_per_tx) {
    CMutableTransaction tx;
    tx.nVersion = 1;
    tx.vin.resize(1);
    tx.vin[0].prevout.hash = GetRandHash();
    tx.vin[0].prevout.n = 0;
    tx.vin[0].scriptSig = CScript();
    tx.vout.resize(1);
    tx.vout[0].nValue = 1;
    tx.vout[0].scriptPubKey = CScript();

    {
        CValidationState state;
        BOOST_CHECK(CheckTransaction(tx, state));
    }

    // Get just before the limit.
    for (size_t i = 0; i < MAX_TX_SIGOPS_COUNT; i++) {
        tx.vout[0].scriptPubKey << OP_CHECKSIG;
    }

    {
        CValidationState state;
        BOOST_CHECK(CheckTransaction(tx, state));
    }

    // And go over.
    tx.vout[0].scriptPubKey << OP_CHECKSIG;

    {
        CValidationState state;
        BOOST_CHECK(!CheckTransaction(tx, state));
        BOOST_CHECK_EQUAL(state.GetRejectReason(), "bad-txn-sigops");
    }
}

/**
 * Verifies script execution of the zeroth scriptPubKey of tx output and
 * zeroth scriptSig of tx input.
 */
ScriptError VerifyWithFlag(const CTransaction& output, const CMutableTransaction& input, int flags)
{
    ScriptError error;
    CTransaction inputi(input);
    bool ret = VerifyScript(inputi.vin[0].scriptSig,
                            output.vout[0].scriptPubKey,
                            flags, TransactionSignatureChecker(&inputi, 0, output.vout[0].nValue), &error);
    BOOST_CHECK((ret == true) == (error == SCRIPT_ERR_OK));

    return error;
}

/**
 * Builds a creationTx from scriptPubKey and a spendingTx from scriptSig
 * such that spendingTx spends output zero of creationTx.
 * Also inserts creationTx's output into the coins view.
 */
void BuildTxs(CMutableTransaction& spendingTx, CCoinsViewCache& coins,
              CMutableTransaction& creationTx, const CScript& scriptPubKey,
              const CScript& scriptSig)
{
    creationTx.nVersion = 1;
    creationTx.vin.resize(1);
    creationTx.vin[0].prevout.SetNull();
    creationTx.vin[0].scriptSig = CScript();
    creationTx.vout.resize(1);
    creationTx.vout[0].nValue = 1;
    creationTx.vout[0].scriptPubKey = scriptPubKey;

    spendingTx.nVersion = 1;
    spendingTx.vin.resize(1);
    spendingTx.vin[0].prevout.hash = creationTx.GetHash();
    spendingTx.vin[0].prevout.n = 0;
    spendingTx.vin[0].scriptSig = scriptSig;
    spendingTx.vout.resize(1);
    spendingTx.vout[0].nValue = 1;
    spendingTx.vout[0].scriptPubKey = CScript();

    AddCoins(coins, CTransaction(creationTx), 0);
}

BOOST_AUTO_TEST_CASE(GetTxSigOpCost)
{
    // Transaction creates outputs
    CMutableTransaction creationTx;
    // Transaction that spends outputs and whose
    // sig op cost is going to be tested
    CMutableTransaction spendingTx;

    // Create utxo set
    CCoinsView coinsDummy;
    CCoinsViewCache coins(&coinsDummy);
    // Create key
    CKey key;
    key.MakeNewKey(true);
    CPubKey pubkey = key.GetPubKey();
    // Default flags
    int flags = SCRIPT_VERIFY_P2SH;

    // Multisig script (legacy counting)
    {
        CScript scriptPubKey = CScript() << 1 << ToByteVector(pubkey) << ToByteVector(pubkey) << 2 << OP_CHECKMULTISIGVERIFY;
        // Do not use a valid signature to avoid using wallet operations.
        CScript scriptSig = CScript() << OP_0 << OP_0;

        BuildTxs(spendingTx, coins, creationTx, scriptPubKey, scriptSig);
        // Legacy counting only includes signature operations in scriptSigs and scriptPubKeys
        // of a transaction and does not take the actual executed sig operations into account.
        // spendingTx in itself does not contain a signature operation.
        BOOST_CHECK(GetLegacySigOpCount(CTransaction(spendingTx), flags) == 0);
        // creationTx contains two signature operations in its scriptPubKey, but legacy counting
        // is not accurate.
        BOOST_CHECK(GetLegacySigOpCount(CTransaction(creationTx), flags) == MAX_PUBKEYS_PER_MULTISIG);
        // Sanity check: script verification fails because of an invalid signature.
        BOOST_CHECK(VerifyWithFlag(creationTx, spendingTx, flags) == SCRIPT_ERR_CHECKMULTISIGVERIFY);
    }

    // Multisig nested in P2SH
    {
        CScript redeemScript = CScript() << 1 << ToByteVector(pubkey) << ToByteVector(pubkey) << 2 << OP_CHECKMULTISIGVERIFY;
        CScript scriptPubKey = GetScriptForDestination(CScriptID(redeemScript));
        CScript scriptSig = CScript() << OP_0 << OP_0 << ToByteVector(redeemScript);

        BuildTxs(spendingTx, coins, creationTx, scriptPubKey, scriptSig);
        BOOST_CHECK(GetP2SHSigOpCount(CTransaction(spendingTx), coins, flags) == 2);
        BOOST_CHECK(VerifyWithFlag(creationTx, spendingTx, flags) == SCRIPT_ERR_CHECKMULTISIGVERIFY);

        // Make sure P2SH sigops are not counted if the flag for P2SH is not
        // passed in.
        BOOST_CHECK_EQUAL(GetP2SHSigOpCount(CTransaction(spendingTx),
                    coins, SCRIPT_VERIFY_NONE), 0);
    }
}

BOOST_AUTO_TEST_SUITE_END()
