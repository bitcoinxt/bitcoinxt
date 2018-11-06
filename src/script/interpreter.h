// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_SCRIPT_INTERPRETER_H
#define BITCOIN_SCRIPT_INTERPRETER_H

#include "script/script_flags.h"
#include "script_error.h"
#include "primitives/transaction.h"

#include <vector>
#include <stdint.h>
#include <string>

class CPubKey;
class CScript;
class CTransaction;
class uint256;
enum class SigHashType : uint32_t;

bool CheckPubKeyEncoding(const std::vector<unsigned char> &vchPubKey, unsigned int flags, ScriptError *serror);

struct PrecomputedTransactionData
{
    uint256 hashPrevouts, hashSequence, hashOutputs;

    PrecomputedTransactionData()
        : hashPrevouts(), hashSequence(), hashOutputs()
    {
    }

    PrecomputedTransactionData(const CTransaction& tx);
};
uint256 SignatureHash(const CScript &scriptCode, const CTransaction &txTo,
                        unsigned int nIn, SigHashType nHashType, const CAmount &amount,
                        unsigned int flags = SCRIPT_ENABLE_SIGHASH_FORKID,
                        const PrecomputedTransactionData* cache = nullptr);

class BaseSignatureChecker
{
public:
    virtual bool VerifySignature(const std::vector<uint8_t> &vchSig,
                                 const CPubKey &vchPubKey,
                                 const uint256 &sighash) const;

    virtual bool CheckSig(const std::vector<unsigned char>& scriptSig, const std::vector<unsigned char>& vchPubKey,
                          const CScript& scriptCode, unsigned int flags) const
    {
        return false;
    }

    virtual bool CheckLockTime(const CScriptNum& nLockTime) const
    {
         return false;
    }

    virtual bool CheckSequence(const CScriptNum& nSequence) const
    {
         return false;
    }

    virtual ~BaseSignatureChecker() {}
};

class TransactionSignatureChecker : public BaseSignatureChecker
{
private:
    const CTransaction* txTo;
    unsigned int nIn;
    const CAmount amount;
    const PrecomputedTransactionData* txdata;

public:
    TransactionSignatureChecker(const CTransaction* txToIn, unsigned int nInIn, const CAmount& amountIn) : txTo(txToIn), nIn(nInIn), amount(amountIn), txdata(nullptr) {}
    TransactionSignatureChecker(const CTransaction* txToIn, unsigned int nInIn, const CAmount& amountIn, const PrecomputedTransactionData& txdataIn) : txTo(txToIn), nIn(nInIn), amount(amountIn), txdata(&txdataIn) {}
    // The overriden functions are now final.
    bool CheckSig(const std::vector<unsigned char>& scriptSig,
            const std::vector<unsigned char>& vchPubKey,
            const CScript& scriptCode, unsigned int flags) const final override;
    bool CheckLockTime(const CScriptNum& nLockTime) const final override;
    bool CheckSequence(const CScriptNum& nSequence) const final override;
};

class MutableTransactionSignatureChecker : public TransactionSignatureChecker
{
private:
    const CTransaction txTo;

public:
    MutableTransactionSignatureChecker(const CMutableTransaction* txToIn, unsigned int nInIn, const CAmount &amount) : TransactionSignatureChecker(&txTo, nInIn, amount), txTo(*txToIn) {}
};

bool EvalScript(std::vector<std::vector<unsigned char> >& stack, const CScript& script, unsigned int flags, const BaseSignatureChecker& checker, ScriptError* error = NULL);
bool VerifyScript(const CScript& scriptSig, const CScript& scriptPubKey, unsigned int flags, const BaseSignatureChecker& checker, ScriptError* error = NULL);

#endif // BITCOIN_SCRIPT_INTERPRETER_H
