// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_CONSENSUS_PARAMS_H
#define BITCOIN_CONSENSUS_PARAMS_H

#include "uint256.h"
#include <map>
#include <string>

namespace Consensus {

enum DeploymentPos
{
    DEPLOYMENT_CSV = 0, // Deployment of BIP68, BIP112, and BIP113.
    DEPLOYMENT_TESTDUMMY = 28,
    MAX_VERSION_BITS_DEPLOYMENTS = 29
};

/**
 * Struct for each individual consensus rule change using BIP135.
 */
struct ForkDeployment
{
    /** Deployment name */
    const char *name;
    /** Whether GBT clients can safely ignore this rule in simplified usage */
    bool gbt_force;
    /** Start MedianTime for version bits miner confirmation. Can be a date in the past */
    int64_t nStartTime;
    /** Timeout/expiry MedianTime for the deployment attempt. */
    int64_t nTimeout;
    /** Window size (in blocks) for generalized versionbits signal tallying */
    int windowsize;
    /** Threshold (in blocks / window) for generalized versionbits lock-in */
    int threshold;
    /** Minimum number of blocks to remain in locked-in state */
    int minlockedblocks;
    /** Minimum duration (in seconds based on MTP) to remain in locked-in state */
    int64_t minlockedtime;
};

/**
 * Parameters that influence chain consensus.
 */
struct Params {
    uint256 hashGenesisBlock;
    int nSubsidyHalvingInterval;
    /** Block height and hash at which BIP34 becomes active */
    int BIP34Height;
    uint256 BIP34Hash;
    /** Block height at which BIP65 becomes active */
    int BIP65Height;
    /** Block height at which BIP66 becomes active */
    int BIP66Height;
    /** Defined BIP135 deployments. */
    std::map<DeploymentPos, ForkDeployment> vDeployments;
    /**
     * BIP100: One-based position from beginning (end) of the ascending sorted list of max block size
     * votes in a retarget interval, at which the possible new lower (higher) max block size is read.
     * 1512 = 75th percentile of 2016
     */
    int bip100ActivationHeight;
    uint32_t nMaxBlockSizeChangePosition;
    /** Proof of work parameters */
    uint256 powLimit;
    bool fPowAllowMinDifficultyBlocks;
    bool fPowNoRetargeting;
    int64_t nPowTargetSpacing;
    int64_t nPowTargetTimespan;
    int64_t DifficultyAdjustmentInterval() const { return nPowTargetTimespan / nPowTargetSpacing; }

    /** Activation time at which the cash HF kicks in. */
    int64_t cashHardForkActivationTime;
};
} // namespace Consensus

#endif // BITCOIN_CONSENSUS_PARAMS_H
