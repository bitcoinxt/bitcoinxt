// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Copyright (c) 2017-2018 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_SCRIPT_SIGENCODING_H
#define BITCOIN_SCRIPT_SIGENCODING_H

#include "script/sighashtype.h"
#include "script/script_error.h"

#include <vector>

inline SigHashType GetHashType(const std::vector<unsigned char> &vchSig) {
    if (vchSig.size() == 0) {
        return SigHashType::UNDEFINED;
    }

    return SigHashType(vchSig[vchSig.size() - 1]);
}

/**
 * Check that the signature provided on some data is properly encoded.
 * Signatures passed to OP_CHECKDATASIG and its verify variant must be checked
 * using this function.
 */
bool CheckDataSignatureEncoding(const std::vector<unsigned char> &vchSig, uint32_t flags,
                                ScriptError *serror);

/**
 * Check that the signature provided to authentify a transaction is properly
 * encoded. Signatures passed to OP_CHECKSIG, OP_CHECKMULTISIG and their verify
 * variants must be checked using this function.
 */
bool CheckTransactionSignatureEncoding(const std::vector<unsigned char> &vchSig, uint32_t flags,
                                       ScriptError *serror);

#endif
