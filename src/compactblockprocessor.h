#ifndef COMPACTBLOCKPROCESSOR_H
#define COMPACTBLOCKPROCESSOR_H

#include "blockprocessor.h"

class CDataStream;
class CTxMemPool;

class CompactBlockProcessor : public BlockProcessor {
    public:

        CompactBlockProcessor(CNode& f, ThinBlockWorker& w, BlockHeaderProcessor& h) :
            BlockProcessor(f, w, "cmpctblock", h)
        {
        }

        void operator()(CDataStream& vRecv, const CTxMemPool& mempool);
};

#endif
