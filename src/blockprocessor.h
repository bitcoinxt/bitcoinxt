#ifndef BLOCKPROCESSOR_H
#define BLOCKPROCESSOR_H

#include <string>
class CConnman;
class CNode;
class ThinBlockWorker;
class BlockHeaderProcessor;
class CBlockHeader;
class uint256;
class CBlockIndex;

class BlockProcessor {

    public:
        BlockProcessor(CConnman& c, CNode& f, ThinBlockWorker& w,
                const std::string& netcmd, BlockHeaderProcessor& h) :
            connman(c), from(f), worker(w), netcmd(netcmd), headerProcessor(h)
        {
        }

        virtual ~BlockProcessor() = 0;
        void rejectBlock(const uint256& block, const std::string& reason, int misbehave);
        bool setToWork(const CBlockHeader& hash, int activeChainHeight);

    protected:
        CConnman& connman;
        CNode& from;
        ThinBlockWorker& worker;
        virtual void misbehave(int howmuch, const std::string& what);
        bool requestConnectHeaders(const CBlockHeader&, bool bumpUnconnecting);
        CBlockIndex* processHeader(const CBlockHeader& header, bool maybeAnnouncement);

    private:
        std::string netcmd;
        BlockHeaderProcessor& headerProcessor;

};

inline BlockProcessor::~BlockProcessor() { }

#endif
