#include "utilhash.h"
#include "random.h"

#include <limits>

SaltedTxIDHasher::SaltedTxIDHasher() :
    k0(GetRand(std::numeric_limits<uint64_t>::max())),
    k1(GetRand(std::numeric_limits<uint64_t>::max()))
{
}
