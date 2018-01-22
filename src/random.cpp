// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "random.h"

#include "support/cleanse.h"
#ifdef WIN32
#include "compat.h" // for Windows API
#endif
#include "serialize.h"        // for begin_ptr(vec)
#include "util.h"             // for LogPrint()
#include "utilstrencodings.h" // for GetTime()

#include <limits>

#ifndef WIN32
#include <sys/time.h>
#endif

#include "sodium.h"

void GetRandBytes(unsigned char* buf, size_t num)
{
    randombytes_buf(buf, num);
}

template <typename RNG>
uint64_t GetRand(uint64_t nMax, RNG rng)
{
    if (nMax == 0)
        return 0;

    // The range of the random source must be a multiple of the modulus
    // to give every possible output value an equal possibility
    uint64_t nRange = (std::numeric_limits<uint64_t>::max() / nMax) * nMax;
    uint64_t nRand = 0;
    do {
        nRand = rng();
    } while (nRand >= nRange);
    return (nRand % nMax);
}

uint64_t GetRand(uint64_t nMax) {
    return GetRand(nMax, [](){
        uint64_t nRand = 0;
        GetRandBytes((unsigned char*)&nRand, sizeof(nRand));
        return nRand;
    });
}

int GetRandInt(int nMax)
{
    return GetRand(nMax);
}

uint256 GetRandHash()
{
    uint256 hash;
    GetRandBytes((unsigned char*)&hash, sizeof(hash));
    return hash;
}

FastRandomContext::FastRandomContext(bool fDeterministic)
{
    // The seed values have some unlikely fixed points which we avoid.
    if (fDeterministic) {
        Rz = Rw = 11;
    } else {
        uint32_t tmp;
        do {
            GetRandBytes((unsigned char*)&tmp, 4);
        } while (tmp == 0 || tmp == 0x9068ffffU);
        Rz = tmp;
        do {
            GetRandBytes((unsigned char*)&tmp, 4);
        } while (tmp == 0 || tmp == 0x464fffffU);
        Rw = tmp;
    }
}

uint64_t FastRandomContext::randrange(uint64_t range) {
    return GetRand(range, [this](){
        return this->rand64();
    });
}
