#ifndef BITCOIN_UTILPROCESSMSG_H
#define BITCOIN_UTILPROCESSMSG_H

#include <vector>
class uint256;
class CNode;
class CBlockHeader;

bool HaveBlockData(const uint256& hash);

#endif
