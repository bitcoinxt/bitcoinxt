// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_CONSENSUS_CONSENSUS_H
#define BITCOIN_CONSENSUS_CONSENSUS_H

#include <stdint.h>

/** Legacy maximum block size */
static const unsigned int MAX_BLOCK_SIZE = 1000000;
/** UAHF initial block size */
static const unsigned int UAHF_INITIAL_MAX_BLOCK_SIZE = 8000000;
/** Initial block size of the third BCH hard fork */
static const unsigned int THIRD_HF_INITIAL_MAX_BLOCK_SIZE = 32000000;
/** The maximum allowed size for a serialized transaction, in bytes */
static const unsigned int MAX_TRANSACTION_SIZE = 1000*1000;
/** The minimum allowed size for a transaction, in bytes */
static const uint64_t MIN_TRANSACTION_SIZE = 100;
/** The maximum allowed number of signature check operations in a block (network rule) */
inline uint64_t MaxBlockSigops(uint64_t nBlockSize) {
    return ((nBlockSize - 1) / 1000000 + 1) * 1000000 / 50;
}
/** allowed number of signature check operations per transaction. */
static const uint64_t MAX_TX_SIGOPS_COUNT = 20000;
/** Coinbase transaction outputs can only be spent after this number of new blocks (network rule) */
static const int COINBASE_MATURITY = 100;

/** Flags for nSequence and nLockTime locks */
enum {
    /* Interpret sequence numbers as relative lock-time constraints. */
    LOCKTIME_VERIFY_SEQUENCE = (1 << 0),

    /* Use GetMedianTimePast() instead of nTime for end point timestamp. */
    LOCKTIME_MEDIAN_TIME_PAST = (1 << 1),
};

#endif // BITCOIN_CONSENSUS_CONSENSUS_H
