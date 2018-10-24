// Copyright (c) 2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "bench.h"
#include "key.h"
#if defined(HAVE_CONSENSUS_LIB)
#include "script/bitcoinconsensus.h"
#endif
#include "script/script.h"
#include "script/sighashtype.h"
#include "script/sign.h"
#include "streams.h"

// FIXME: Dedup with BuildCreditingTransaction in test/script_tests.cpp.
static CMutableTransaction BuildCreditingTransaction(const CScript& scriptPubKey)
{
    CMutableTransaction txCredit;
    txCredit.nVersion = 1;
    txCredit.nLockTime = 0;
    txCredit.vin.resize(1);
    txCredit.vout.resize(1);
    txCredit.vin[0].prevout.SetNull();
    txCredit.vin[0].scriptSig = CScript() << CScriptNum(0) << CScriptNum(0);
    txCredit.vin[0].nSequence = CTxIn::SEQUENCE_FINAL;
    txCredit.vout[0].scriptPubKey = scriptPubKey;
    txCredit.vout[0].nValue = 1;

    return txCredit;
}

// FIXME: Dedup with BuildSpendingTransaction in test/script_tests.cpp.
static CMutableTransaction BuildSpendingTransaction(const CScript& scriptSig, const CMutableTransaction& txCredit)
{
    CMutableTransaction txSpend;
    txSpend.nVersion = 1;
    txSpend.nLockTime = 0;
    txSpend.vin.resize(1);
    txSpend.vout.resize(1);
    txSpend.vin[0].prevout.hash = txCredit.GetHash();
    txSpend.vin[0].prevout.n = 0;
    txSpend.vin[0].scriptSig = scriptSig;
    txSpend.vin[0].nSequence = CTxIn::SEQUENCE_FINAL;
    txSpend.vout[0].scriptPubKey = CScript();
    txSpend.vout[0].nValue = txCredit.vout[0].nValue;

    return txSpend;
}

// Microbenchmark for verification of a basic P2PKH script. Can be easily
// modified to measure performance of other types of scripts.
static void VerifyScriptBench(benchmark::State& state)
{
    const int flags = SCRIPT_ENABLE_SIGHASH_FORKID;

    // Keypair.
    CKey key;
    const unsigned char vchKey[32] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1};
    key.Set(vchKey, vchKey + 32, false);
    CPubKey pubkey = key.GetPubKey();
    uint160 pubkeyHash;
    CHash160().Write(pubkey.begin(), pubkey.size()).Finalize(pubkeyHash.begin());

    // Script.
    CScript scriptSig;
    CScript scriptPubKey = CScript() << OP_DUP << OP_HASH160 << ToByteVector(pubkeyHash) << OP_EQUALVERIFY << OP_CHECKSIG;
    CTransaction txCredit = BuildCreditingTransaction(scriptPubKey);
    CMutableTransaction txSpend = BuildSpendingTransaction(scriptSig, txCredit);

    uint256 hash = SignatureHash(scriptPubKey, txSpend, 0, SigHashType::ALL | SigHashType::FORKID, txCredit.vout[0].nValue);
    std::vector<uint8_t> sig;
    if (!key.Sign(hash, sig)) {
        assert(!"sign failed");
    }
    sig.push_back(static_cast<uint8_t>(SigHashType::ALL | SigHashType::FORKID));
    txSpend.vin[0].scriptSig = CScript() << sig << ToByteVector(pubkey);

    // Benchmark.
    while (state.KeepRunning()) {
        ScriptError err;
        bool success = VerifyScript(
            txSpend.vin[0].scriptSig,
            txCredit.vout[0].scriptPubKey,
            flags,
            MutableTransactionSignatureChecker(&txSpend, 0, txCredit.vout[0].nValue),
            &err);
        assert(err == SCRIPT_ERR_OK);
        assert(success);

#if defined(HAVE_CONSENSUS_LIB)
        CDataStream stream(SER_NETWORK, PROTOCOL_VERSION);
        stream << txSpend;
        int csuccess = bitcoinconsensus_verify_script_with_amount(
            begin_ptr(txCredit.vout[0].scriptPubKey),
            txCredit.vout[0].scriptPubKey.size(),
            txCredit.vout[0].nValue,
            (const unsigned char*)&stream[0], stream.size(), 0, flags, nullptr);
        assert(csuccess == 1);
#endif
    }
}

BENCHMARK(VerifyScriptBench);
