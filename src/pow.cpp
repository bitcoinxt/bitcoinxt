// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "pow.h"

#include "arith_uint256.h"
#include "chain.h"
#include "options.h"
#include "primitives/block.h"
#include "uint256.h"
#include "util.h"

/**
 * Compute the next required proof of work using the legacy Bitcoin difficulty
 * adjustement + Emergency Difficulty Adjustement (EDA).
 */
static uint32_t GetNextEDAWorkRequired(const CBlockIndex *pindexLast,
                                       const CBlockHeader *pblock,
                                       const Consensus::Params &params) {
    // Only change once per difficulty adjustment interval
    uint32_t nHeight = pindexLast->nHeight + 1;
    if (nHeight % params.DifficultyAdjustmentInterval() == 0) {
        // Go back by what we want to be 14 days worth of blocks
        assert(nHeight >= params.DifficultyAdjustmentInterval());
        uint32_t nHeightFirst = nHeight - params.DifficultyAdjustmentInterval();
        const CBlockIndex *pindexFirst = pindexLast->GetAncestor(nHeightFirst);
        assert(pindexFirst);

        return CalculateNextWorkRequired(pindexLast,
                                         pindexFirst->GetBlockTime(), params);
    }

    const uint32_t nProofOfWorkLimit =
        UintToArith256(params.powLimit).GetCompact();

    if (params.fPowAllowMinDifficultyBlocks) {
        // Special difficulty rule for testnet:
        // If the new block's timestamp is more than 2* 10 minutes then allow
        // mining of a min-difficulty block.
        if (pblock->GetBlockTime() >
            pindexLast->GetBlockTime() + 2 * params.nPowTargetSpacing) {
            return nProofOfWorkLimit;
        }

        // Return the last non-special-min-difficulty-rules-block
        const CBlockIndex *pindex = pindexLast;
        while (pindex->pprev &&
               pindex->nHeight % params.DifficultyAdjustmentInterval() != 0 &&
               pindex->nBits == nProofOfWorkLimit) {
            pindex = pindex->pprev;
        }

        return pindex->nBits;
    }

    // Do not go lower than the POW limit.
    uint32_t nBits = pindexLast->nBits;
    if (nBits == nProofOfWorkLimit) {
        return nProofOfWorkLimit;
    }

    // Done, unless checking UAHF fast difficulty drop
    if ((Opt().UAHFTime() == 0) || (pindexLast->GetMedianTimePast() < Opt().UAHFTime()))
        return nBits;

    // If producing the last 6 block took less than 12h, we keep the same difficulty.
    const CBlockIndex *pindex6 = pindexLast->GetAncestor(nHeight - 7);
    assert(pindex6);
    int64_t mtp6blocks =
        pindexLast->GetMedianTimePast() - pindex6->GetMedianTimePast();
    if (mtp6blocks < 12 * 3600) {
        return nBits;
    }

    // If producing the last 6 block took more than 12h, increase the difficulty
    // target by 25% (which reduces the difficulty by 20%). This ensure the
    // chain do not get stuck in case we lose hashrate abruptly.
    arith_uint256 nPow;
    nPow.SetCompact(nBits);
    nPow += (nPow >> 2);

    // Make sure we do not go below allowed values.
    const arith_uint256 bnPowLimit = UintToArith256(params.powLimit);
    if (nPow > bnPowLimit) nPow = bnPowLimit;

    return nPow.GetCompact();
}

unsigned int GetNextWorkRequired(const CBlockIndex *pindexPrev,
                             const CBlockHeader *pblock,
                             const Consensus::Params &params) {
    // Genesis block
    if (pindexPrev == nullptr) {
        return UintToArith256(params.powLimit).GetCompact();
    }

    // Special rule for regtest: we never retarget.
    if (params.fPowNoRetargeting) {
        return pindexPrev->nBits;
    }

    return GetNextEDAWorkRequired(pindexPrev, pblock, params);
}

unsigned int CalculateNextWorkRequired(const CBlockIndex* pindexLast, int64_t nFirstBlockTime, const Consensus::Params& params)
{
    if (params.fPowNoRetargeting)
        return pindexLast->nBits;

    // Limit adjustment step
    int64_t nActualTimespan = pindexLast->GetBlockTime() - nFirstBlockTime;
    LogPrintf("  nActualTimespan = %d  before bounds\n", nActualTimespan);
    if (nActualTimespan < params.nPowTargetTimespan/4)
        nActualTimespan = params.nPowTargetTimespan/4;
    if (nActualTimespan > params.nPowTargetTimespan*4)
        nActualTimespan = params.nPowTargetTimespan*4;

    // Retarget
    const arith_uint256 bnPowLimit = UintToArith256(params.powLimit);
    arith_uint256 bnNew;
    arith_uint256 bnOld;
    bnNew.SetCompact(pindexLast->nBits);
    bnOld = bnNew;
    bnNew *= nActualTimespan;
    bnNew /= params.nPowTargetTimespan;

    if (bnNew > bnPowLimit)
        bnNew = bnPowLimit;

    /// debug print
    LogPrintf("GetNextWorkRequired RETARGET\n");
    LogPrintf("params.nPowTargetTimespan = %d    nActualTimespan = %d\n", params.nPowTargetTimespan, nActualTimespan);
    LogPrintf("Before: %08x  %s\n", pindexLast->nBits, bnOld.ToString());
    LogPrintf("After:  %08x  %s\n", bnNew.GetCompact(), bnNew.ToString());

    return bnNew.GetCompact();
}

bool CheckProofOfWork(uint256 hash, unsigned int nBits, const Consensus::Params& params)
{
    bool fNegative;
    bool fOverflow;
    arith_uint256 bnTarget;

    bnTarget.SetCompact(nBits, &fNegative, &fOverflow);

    // Check range
    if (fNegative || bnTarget == 0 || fOverflow || bnTarget > UintToArith256(params.powLimit))
        return error("CheckProofOfWork(): nBits below minimum work");

    // Check proof of work matches claimed amount
    if (UintToArith256(hash) > bnTarget)
        return error("CheckProofOfWork(): hash doesn't match nBits");

    return true;
}

arith_uint256 GetBlockProof(const CBlockIndex& block)
{
    arith_uint256 bnTarget;
    bool fNegative;
    bool fOverflow;
    bnTarget.SetCompact(block.nBits, &fNegative, &fOverflow);
    if (fNegative || fOverflow || bnTarget == 0)
        return 0;
    // We need to compute 2**256 / (bnTarget+1), but we can't represent 2**256
    // as it's too large for a arith_uint256. However, as 2**256 is at least as large
    // as bnTarget+1, it is equal to ((2**256 - bnTarget - 1) / (bnTarget+1)) + 1,
    // or ~bnTarget / (nTarget+1) + 1.
    return (~bnTarget / (bnTarget + 1)) + 1;
}

int64_t GetBlockProofEquivalentTime(const CBlockIndex& to, const CBlockIndex& from, const CBlockIndex& tip, const Consensus::Params& params)
{
    arith_uint256 r;
    int sign = 1;
    if (to.nChainWork > from.nChainWork) {
        r = to.nChainWork - from.nChainWork;
    } else {
        r = from.nChainWork - to.nChainWork;
        sign = -1;
    }
    r = r * arith_uint256(params.nPowTargetSpacing) / GetBlockProof(tip);
    if (r.bits() > 63) {
        return sign * std::numeric_limits<int64_t>::max();
    }
    return sign * r.GetLow64();
}
