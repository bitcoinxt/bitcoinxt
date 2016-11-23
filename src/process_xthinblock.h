#ifndef PROCESS_XTHINBLOCK_H
#define PROCESS_XTHINBLOCK_H

#include "blockprocessor.h"

class CDataStream;
class TxFinder;

class XThinBlockProcessor : private BlockProcessor {
    public:
        XThinBlockProcessor(CNode& f, ThinBlockWorker& w, BlockHeaderProcessor& h) :
            BlockProcessor(f, w, "xthinblock", h)
        {
        }

        void operator()(CDataStream& vRecv, const TxFinder& txfinder);
};

#endif
