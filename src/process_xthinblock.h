#ifndef PROCESS_XTHINBLOCK_H
#define PROCESS_XTHINBLOCK_H

#include "blockprocessor.h"

class CDataStream;
class TxFinder;

class XThinBlockProcessor : private BlockProcessor {
    public:
        XThinBlockProcessor(CConnman& c, CNode& f, ThinBlockWorker& w, BlockHeaderProcessor& h) :
            BlockProcessor(c, f, w, "xthinblock", h)
        {
        }

        void operator()(CDataStream& vRecv, const TxFinder& txfinder,
                uint64_t currMaxBlockSize, int activeChainHeight);
};

#endif
