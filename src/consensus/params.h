// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_CONSENSUS_PARAMS_H
#define BITCOIN_CONSENSUS_PARAMS_H

#include "uint256.h"
#include <limits>

namespace Consensus {
/**
 * Parameters that influence chain consensus.
 */
struct Params {
    uint256 hashGenesisBlock;
    int nSubsidyHalvingInterval;
    /** Used to check majorities for block version upgrade */
    int nMajorityEnforceBlockUpgrade;
    int nMajorityRejectBlockOutdated;
    int nMajorityWindow;
    /** Proof of work parameters */
    uint256 powLimit;
    bool fPowAllowMinDifficultyBlocks;
    int64_t nPowTargetSpacing;
    int64_t nPowTargetTimespan;
    int64_t DifficultyAdjustmentInterval() const { return nPowTargetTimespan / nPowTargetSpacing; }

    /** Maximum block size parameters */
    uint32_t nMaxSizePreFork;
    uint64_t nEarliestSizeForkTime;
    uint32_t nSizeDoubleEpoch;
    uint64_t nMaxSizeBase;
    uint8_t nMaxSizeDoublings;
    int nActivateSizeForkMajority;
    uint64_t nSizeForkGracePeriod;

    /** Maximum block size of a block with timestamp nBlockTimestamp */
    uint64_t MaxBlockSize(uint64_t nBlockTimestamp, uint64_t nSizeForkActivationTime) const {
        if (nBlockTimestamp < nEarliestSizeForkTime || nBlockTimestamp < nSizeForkActivationTime)
            return nMaxSizePreFork;
        if (nBlockTimestamp >= nEarliestSizeForkTime + nSizeDoubleEpoch * nMaxSizeDoublings)
            return nMaxSizeBase << nMaxSizeDoublings;

        // Piecewise-linear-between-doublings growth. Calculated based on a fixed
        // timestamp and not the activation time so the maximum size is
        // predictable, and so the activation time can be completely removed in
        // a future version of this code after the fork is complete.
        uint64_t timeDelta = nBlockTimestamp - nEarliestSizeForkTime;
        uint64_t doublings = timeDelta / nSizeDoubleEpoch;
        uint64_t remain = timeDelta % nSizeDoubleEpoch;
        uint64_t interpolate = (nMaxSizeBase << doublings) * remain / nSizeDoubleEpoch;
        uint64_t nMaxSize = (nMaxSizeBase << doublings) + interpolate;
        return nMaxSize;
    }

    // Signature-operation-counting is a CPU exhaustion denial-of-service prevention
    // measure. Prior to the maximum block size fork it was done in two different, ad-hoc,
    // inaccurate ways.
    // Post-fork it is done in an accurate way, counting how many ECDSA verify operations
    // and how many bytes must be hashed to compute signature hashes to validate a block.

    /** Pre-fork consensus rules use an inaccurate method of counting sigops **/
    uint64_t MaxBlockLegacySigops(uint64_t nBlockTimestamp, uint64_t nSizeForkActivationTime) const {
        if (nBlockTimestamp < nEarliestSizeForkTime || nBlockTimestamp < nSizeForkActivationTime)
            return MaxBlockSize(nBlockTimestamp, nSizeForkActivationTime)/50;
        return std::numeric_limits<uint64_t>::max(); // Post-fork uses accurate method
    }
    //
    // MaxBlockSize/100 was chosen for number of sigops (ECDSA verifications) because
    // a single ECDSA signature verification requires a public key (33 bytes) plus
    // a signature (~72 bytes), so allowing one sigop per 100 bytes should allow any
    // reasonable set of transactions (but will prevent 'attack' transactions that
    // just try to use as much CPU as possible in as few bytes as possible).
    //
    uint64_t MaxBlockAccurateSigops(uint64_t nBlockTimestamp, uint64_t nSizeForkActivationTime) const {
        if (nBlockTimestamp < nEarliestSizeForkTime || nBlockTimestamp < nSizeForkActivationTime)
            return std::numeric_limits<uint64_t>::max(); // Pre-fork doesn't care
        return MaxBlockSize(nBlockTimestamp, nSizeForkActivationTime)/100;
    }
    //
    // MaxBlockSize*160 was chosen for maximum number of bytes hashed so any possible
    // non-attack one-megabyte-large transaction that might have been signed and
    // saved before the fork could still be mined after the fork. A 5,000-SIGHASH_ALL-input,
    // single-output, 999,000-byte transaction requires about 1.2 gigabytes of hashing
    // to compute those 5,000 signature hashes.
    //
    // Note that such a transaction was, and is, considered "non-standard" because it is
    // over 100,000 bytes big.
    //
    uint64_t MaxBlockSighashBytes(uint64_t nBlockTimestamp, uint64_t nSizeForkActivationTime) const {
        if (nBlockTimestamp < nEarliestSizeForkTime || nBlockTimestamp < nSizeForkActivationTime)
            return std::numeric_limits<uint64_t>::max(); // Pre-fork doesn't care
        return MaxBlockSize(nBlockTimestamp, nSizeForkActivationTime)*160;
    }

    int ActivateSizeForkMajority() const { return nActivateSizeForkMajority; }
    uint64_t SizeForkGracePeriod() const { return nSizeForkGracePeriod; }
};
} // namespace Consensus

#endif // BITCOIN_CONSENSUS_PARAMS_H
