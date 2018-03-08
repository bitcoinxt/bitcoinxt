// Copyright (c) 2017 The Bitcoin XT developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef BITCOIN_UTILFORK_H
#define BITCOIN_UTILFORK_H

#include <cstdint>
class CBlockIndex;

bool IsUAHFActivatingBlock(int64_t mtpCurrent, CBlockIndex* pindexPrev);
bool IsThirdHFActivatingBlock(int64_t mtpCurrent, CBlockIndex* pindexPrev);
bool IsThirdHFActive(int64_t mtpChainTip);

#endif
