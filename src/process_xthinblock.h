#ifndef PROCESS_XTHINBLOCK_H
#define PROCESS_XTHINBLOCK_H

class CNode;
class CDataStream;
class ThinBlockWorker;
class TxFinder;
class BlockHeaderProcessor;

class XThinBlockProcessor {
    public:
        XThinBlockProcessor(CNode& n) : pfrom(n) { }
        ~XThinBlockProcessor() { }

        void operator()(CDataStream& vRecv,
            ThinBlockWorker& worker, const TxFinder& txfinder,
            BlockHeaderProcessor& processHeader);


        virtual void misbehave(int howmuch);

    private:
        CNode& pfrom;
};

#endif
