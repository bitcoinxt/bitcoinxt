#include "miner/utilminer.h"

#include "consensus/consensus.h"
#include "primitives/transaction.h"
#include "protocol.h"
#include "serialize.h"

#include <cstddef>

namespace miner {

void BloatCoinbaseSize(CMutableTransaction& coinbase) {
    size_t size = ::GetSerializeSize(coinbase, SER_NETWORK, PROTOCOL_VERSION);
    if (size >= MIN_TRANSACTION_SIZE) {
        return;
    }
    // operator<< prefixes the padding with minimum 1 byte, thus -1
    size_t padding = MIN_TRANSACTION_SIZE - size - 1;
    coinbase.vin[0].scriptSig << std::vector<uint8_t>(padding);
}

} // ns miner
