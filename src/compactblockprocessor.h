#ifndef COMPACTBLOCKPROCESSOR_H
#define COMPACTBLOCKPROCESSOR_H

#include "blockprocessor.h"

class CDataStream;
class TxFinder;

class CompactBlockProcessor : public BlockProcessor {
    public:

        CompactBlockProcessor(CNode& f, ThinBlockWorker& w, BlockHeaderProcessor& h) :
            BlockProcessor(f, w, "cmpctblock", h)
        {
        }

        void operator()(CDataStream& vRecv, const TxFinder& txfinde);
};

#endif
