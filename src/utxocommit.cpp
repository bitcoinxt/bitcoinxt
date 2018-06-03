// Copyright (c) 2017 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "utxocommit.h"

#include "coins.h"
#include "util.h"

namespace {
// Lazy, thread-safe singleton that wraps the secp256k1_context used for the
// multisets
class CSecp256k1Context {
private:
    secp256k1_context *context;

    CSecp256k1Context()
    {
        context = secp256k1_context_create(SECP256K1_CONTEXT_NONE);
    }

    ~CSecp256k1Context() { secp256k1_context_destroy(context); }

public:
    static const secp256k1_context *GetContext() {
        static const CSecp256k1Context instance;
        return instance.context;
    }
    CSecp256k1Context(CSecp256k1Context const &) = delete;
    void operator=(CSecp256k1Context const &) = delete;
};
}

// Constructs empty CUtxoCommit
CUtxoCommit::CUtxoCommit() {
    secp256k1_multiset_init(CSecp256k1Context::GetContext(), &multiset);
}

// Adds a TXO to multiset
void CUtxoCommit::Add(const COutPoint &out, const Coin &element) {
    CDataStream txo(SER_NETWORK, PROTOCOL_VERSION);
    txo << out << element;
    secp256k1_multiset_add(CSecp256k1Context::GetContext(), &multiset,
                           (const uint8_t *)txo.data(), txo.size());
}

// Adds another commitment to this one
void CUtxoCommit::Add(const CUtxoCommit &other) {
    secp256k1_multiset_combine(CSecp256k1Context::GetContext(), &this->multiset,
                               &other.multiset);
}

// Removes a TXO from multiset
void CUtxoCommit::Remove(const COutPoint &out, const Coin &element) {
    CDataStream txo(SER_NETWORK, PROTOCOL_VERSION);
    txo << out << element;
    secp256k1_multiset_remove(CSecp256k1Context::GetContext(), &multiset,
                              (const uint8_t *)&txo[0], txo.size());
}

void CUtxoCommit::Clear() {
    secp256k1_multiset_init(CSecp256k1Context::GetContext(), &multiset);
}

uint256 CUtxoCommit::GetHash() const {
    std::vector<uint8_t> hash(32);
    secp256k1_multiset_finalize(CSecp256k1Context::GetContext(), hash.data(),
                                &multiset);
    return uint256(hash);
}

bool CUtxoCommit::AddCoinView(CCoinsViewCursor *pcursor) {
    LogPrintf("Adding existing UTXO set to the UTXO commitment");

    // TODO: Parallelize
    int n = 0;
    while (pcursor->Valid()) {

        COutPoint key;
        Coin coin;
        if (!pcursor->GetKey(key) || !pcursor->GetValue(coin)) {
            return error("Failed to retrieve UTXO from cursor");
        }

        Add(key, coin);

        if ((n % 1000000) == 0) {
            uint8_t c = *key.hash.begin();
            LogPrintf("Generating UTXO commitment; progress %d\n",
                      uint32_t(c) * 100 / 256);
        }
        n++;

        pcursor->Next();
    }
    return true;
}
