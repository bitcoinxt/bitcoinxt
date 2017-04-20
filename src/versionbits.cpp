// Copyright (c) 2016 The Bitcoin Core developers
// Copyright (c) 2018 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "versionbits.h"

#include "consensus/params.h"

bool IsConfiguredDeployment(const Consensus::Params &consensusParams, const Consensus::DeploymentPos bit) {
    return bool(consensusParams.vDeployments.count(bit));
}

bool AbstractThresholdConditionChecker::BackAtDefined(ThresholdConditionCache &cache, const CBlockIndex *pindex) const
{
    if (pindex == nullptr) {
        return false;
    }
    auto threshold = cache.find(pindex);
    if (threshold == end(cache)) {
        return false;
    }
    return threshold->second == THRESHOLD_DEFINED;
}

ThresholdState AbstractThresholdConditionChecker::GetStateFor(const CBlockIndex *pindexPrev,
    const Consensus::Params &params,
    ThresholdConditionCache &cache) const
{
    int nPeriod = Period(params);
    int nThreshold = Threshold(params);
    int64_t nTimeStart = BeginTime(params);
    int64_t nTimeTimeout = EndTime(params);
    int nMinLockedBlocks = MinLockedBlocks(params);
    int64_t nMinLockedTime = MinLockedTime(params);
    int64_t nActualLockinTime = 0;
    int nActualLockinBlock = 0;

    // A block's state is always the same as that of the first of its period, so
    // it is computed based on a pindexPrev whose height equals a multiple of
    // nPeriod - 1.
    if (pindexPrev != nullptr) {
        pindexPrev = pindexPrev->GetAncestor(pindexPrev->nHeight - ((pindexPrev->nHeight + 1) % nPeriod));
    }

    // Walk backwards in steps of nPeriod to find a pindexPrev which was DEFINED
    std::vector<const CBlockIndex *> vToCompute;

    while (!BackAtDefined(cache, pindexPrev)) {
        if (pindexPrev == nullptr)
        {
            // The genesis block is by definition defined.
            cache[pindexPrev] = THRESHOLD_DEFINED;
            break;
        }
        if (pindexPrev->GetMedianTimePast() < nTimeStart)
        {
            // Optimizaton: don't recompute down further, as we know every
            // earlier block will be before the start time
            cache[pindexPrev] = THRESHOLD_DEFINED;
            break;
        }

        // push the pindex for later forward walking
        vToCompute.push_back(pindexPrev);
        // go back one more period
        pindexPrev = pindexPrev->GetAncestor(pindexPrev->nHeight - nPeriod);
    }

    // At this point, cache[pindexPrev] is known
    assert(cache.count(pindexPrev));

    // initialize starting state for forward walk
    ThresholdState state = cache[pindexPrev];
    assert(state == THRESHOLD_DEFINED);
    // Now walk forward and compute the state of descendants of pindexPrev
    while (!vToCompute.empty()) {
        ThresholdState stateNext = state;
        pindexPrev = vToCompute.back();
        vToCompute.pop_back();

        switch (state) {
            case THRESHOLD_DEFINED: {
                if (pindexPrev->GetMedianTimePast() >= nTimeTimeout) {
                    stateNext = THRESHOLD_FAILED;
                } else if (pindexPrev->GetMedianTimePast() >= nTimeStart) {
                    stateNext = THRESHOLD_STARTED;
                }
                break;
            }
            case THRESHOLD_STARTED: {
                if (pindexPrev->GetMedianTimePast() >= nTimeTimeout) {
                    stateNext = THRESHOLD_FAILED;
                    break;
                }
                // We need to count
                const CBlockIndex *pindexCount = pindexPrev;
                int count = 0;
                for (int i = 0; i < nPeriod; i++) {
                    if (Condition(pindexCount, params)) {
                        count++;
                    }
                    pindexCount = pindexCount->pprev;
                }
                if (count >= nThreshold) {
                    stateNext = THRESHOLD_LOCKED_IN;
                    // make a note of lock-in time & height
                    // this will be used for assessing grace period conditions.
                    nActualLockinBlock = pindexPrev->nHeight;
                    nActualLockinTime = pindexPrev->GetMedianTimePast();
                }
                break;
            }
            case THRESHOLD_LOCKED_IN: {
                // Progress to ACTIVE only once all grace conditions are met.
                if (pindexPrev->GetMedianTimePast() >= nActualLockinTime + nMinLockedTime &&
                    pindexPrev->nHeight >= nActualLockinBlock + nMinLockedBlocks)
                {
                    stateNext = THRESHOLD_ACTIVE;
                }
                else
                {
                    // if grace not yet met, remain in LOCKED_IN
                    stateNext = THRESHOLD_LOCKED_IN;
                }
                break;
            }
            case THRESHOLD_ACTIVE:
            case THRESHOLD_FAILED:
            {
                // Nothing happens, these are terminal states.
                break;
            }
        }
        cache[pindexPrev] = state = stateNext;
    }

    return state;
}


namespace
{
/**
 * Class to implement versionbits logic.
 */
class VersionBitsConditionChecker : public AbstractThresholdConditionChecker {
private:
    const Consensus::DeploymentPos id;

protected:
    int64_t BeginTime(const Consensus::Params &params) const { return params.vDeployments.at(id).nStartTime; }
    int64_t EndTime(const Consensus::Params &params) const { return params.vDeployments.at(id).nTimeout; }
    int Period(const Consensus::Params &params) const { return params.vDeployments.at(id).windowsize; }
    int Threshold(const Consensus::Params &params) const { return params.vDeployments.at(id).threshold; }
    int MinLockedBlocks(const Consensus::Params &params) const { return params.vDeployments.at(id).minlockedblocks; }
    int64_t MinLockedTime(const Consensus::Params &params) const { return params.vDeployments.at(id).minlockedtime; }
    bool Condition(const CBlockIndex *pindex, const Consensus::Params &params) const
    {
        return (((pindex->nVersion & VERSIONBITS_TOP_MASK) == VERSIONBITS_TOP_BITS) && (pindex->nVersion & Mask(params)) != 0);
    }

public:
    VersionBitsConditionChecker(Consensus::DeploymentPos id_) : id(id_) {}
    uint32_t Mask(const Consensus::Params& params) const { return ((uint32_t)1) << id; }
};

}

ThresholdState VersionBitsState(const CBlockIndex* pindexPrev, const Consensus::Params& params, Consensus::DeploymentPos pos, VersionBitsCache& cache)
{
    return VersionBitsConditionChecker(pos).GetStateFor(pindexPrev, params, cache.caches[pos]);
}

uint32_t VersionBitsMask(const Consensus::Params& params, Consensus::DeploymentPos pos)
{
    return VersionBitsConditionChecker(pos).Mask(params);
}


void VersionBitsCache::Clear()
{
    for (unsigned int d = 0; d < Consensus::MAX_VERSION_BITS_DEPLOYMENTS; d++) {
        caches[d].clear();
    }
}
