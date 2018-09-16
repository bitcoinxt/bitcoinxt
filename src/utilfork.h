// Copyright (c) 2017-2018 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef BITCOIN_UTILFORK_H
#define BITCOIN_UTILFORK_H

#include <cstdint>

class CBlockIndex;
class CTxMemPool;

bool IsUAHFActivatingBlock(int64_t mtpCurrent, const CBlockIndex* pindexPrev);
bool IsUAHFActive(uint64_t mtpChainTip);
bool IsThirdHFActivatingBlock(int64_t mtpCurrent, const CBlockIndex* pindexPrev);
bool IsThirdHFActive(int64_t mtpChainTip);

// Nov 2018 upgrade
bool IsFourthHFActivatingBlock(int64_t mtpCurrent, const CBlockIndex* pindexPrev);
bool IsFourthHFActive(int64_t mtpChainTip);

void ForkMempoolClearer(CTxMemPool& mempool,
                        const CBlockIndex* oldTip, const CBlockIndex* newTip);

#endif
