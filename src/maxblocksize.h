// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2017 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_MAXBLOCKSIZE_H
#define BITCOIN_MAXBLOCKSIZE_H

#include "consensus/consensus.h"
#include "consensus/params.h"
#include "script/script.h"

#include <stdint.h>

class CBlockHeader;
class CBlockIndex;

uint64_t GetNextMaxBlockSize(const CBlockIndex* pindexLast, const Consensus::Params&);

uint64_t GetMaxBlockSizeVote(const CScript &coinbase, int32_t nHeight);

// Max potential size for next block. This function gives reliable lower bound,
// but may overshoot drastically.
uint64_t NextBlockRaiseCap(uint64_t maxCurrBlock);

#endif // BITCOIN_MAXBLOCKSIZE_H
