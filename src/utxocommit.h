// Copyright (c) 2018 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_UTXOCOMMIT_H
#define BITCOIN_UTXOCOMMIT_H

#include "hash.h"
#include "secp256k1/include/secp256k1_multiset.h"
#include "streams.h"

#include <vector>

class Coin;
class COutPoint;
class CCoinsViewCursor;

/**
 * A Utxo Commitment
 *
 * This is maintained as 96-byte multiset value that uniquely defines a UTXO set
 *
 * It wraps the secp256k1 multiset
 *
 * Note that a CUtxoCommit allows "negative sets". That is
 *
 * CUtxoCommit set; // set is an empty set
 * set.Remove(X);   // set is empty set "minus" X
 * set.Add(X);      // set is an empty set
 *
 * This means a CUtxoCommit can both represent the total UTXO set, or a delta to
 * the UTXO set
*/
class CUtxoCommit {
private:
    secp256k1_multiset multiset;

public:
    // Constructs empty CUtxoCommit
    CUtxoCommit();

    // Adds a TXO to a the commitment
    void Add(const COutPoint &out, const Coin &element);

    // Adds another commitment to this one
    void Add(const CUtxoCommit &other);

    // Removes a TXO from multiset
    void Remove(const COutPoint &out, const Coin &element);

    void Clear();

    uint256 GetHash() const;

    // Comparison
    friend bool operator==(const CUtxoCommit &a, const CUtxoCommit &b) {
        return a.GetHash() == b.GetHash();
    }
    friend bool operator!=(const CUtxoCommit &a, const CUtxoCommit &b) {
        return a.GetHash() != b.GetHash();
    }

    // Serialization
    template <typename Stream> void Serialize(Stream &s) const {
        s.write((char *)multiset.data, sizeof(multiset));
    }
    template <typename Stream> void Unserialize(Stream &s) {
        s.read((char *)multiset.data, sizeof(multiset));
    }
};

#endif // MULTISET_H
