#ifndef BITCOIN_UTILHASH_H
#define BITCOIN_UTILHASH_H

#include "hash.h"
#include "uint256.h"

class SaltedTxIDHasher
{
private:
    /** Salt */
    const uint64_t k0, k1;

public:
    SaltedTxIDHasher();
    size_t operator()(const uint256& txid) const {
        return SipHashUint256(k0, k1, txid);
    }
};

#endif
