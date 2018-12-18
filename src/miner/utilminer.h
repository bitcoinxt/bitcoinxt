#ifndef BITCOIN_UTILMINER_H
#define BITCOIN_UTILMINER_H

class CMutableTransaction;

namespace miner {

// Make sure coinbase is at minimum MIN_TRANSACTION_SIZE
void BloatCoinbaseSize(CMutableTransaction& coinbase);

} // ns miner

#endif
