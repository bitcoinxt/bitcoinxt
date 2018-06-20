#ifndef COMPACTBLOCKPROCESSOR_H
#define COMPACTBLOCKPROCESSOR_H

#include "blockprocessor.h"

class CDataStream;
class CTxMemPool;

class CompactBlockProcessor : public BlockProcessor {
    public:
        CompactBlockProcessor(CConnman& c, CNode& f, ThinBlockWorker& w, BlockHeaderProcessor& h) :
            BlockProcessor(c, f, w, "cmpctblock", h)
        {
        }

        void operator()(CDataStream& vRecv, const CTxMemPool& mempool,
                uint64_t currMaxBlockSize, int activeChainHeight);
};

#endif
