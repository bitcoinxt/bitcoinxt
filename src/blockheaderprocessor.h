#ifndef BITCOIN_BLOCKHEDERPROCESSOR_H

#include <vector>
#include <functional>
#include <tuple>
#include <stdexcept>

class CNode;
class CBlockHeader;
class CBlockIndex;
class InFlightIndex;
class ThinBlockManager;
class BlockInFlightMarker;

class BlockHeaderError : public std::runtime_error {
    public:
        BlockHeaderError(const std::string& what) : std::runtime_error(what) { }
};

class BlockHeaderProcessor {
    public:
        virtual CBlockIndex* operator()(const std::vector<CBlockHeader>& headers,
                bool peerSentMax,
                bool maybeAnnouncement) = 0;
        virtual ~BlockHeaderProcessor() = 0;
        virtual bool requestConnectHeaders(const CBlockHeader& h, CNode& from,
                                           bool bumpUnconnecting) = 0;
};
inline BlockHeaderProcessor::~BlockHeaderProcessor() { }

/// Process a block header received from another peer on the network.
class DefaultHeaderProcessor : public BlockHeaderProcessor {
    public:

        DefaultHeaderProcessor(CNode* pfrom,
                InFlightIndex&, ThinBlockManager&, BlockInFlightMarker&,
                std::function<void()> checkBlockIndex);

        CBlockIndex* operator()(const std::vector<CBlockHeader>& headers,
                bool peerSentMax,
                bool maybeAnnouncement) override;

        bool requestConnectHeaders(const CBlockHeader& h, CNode& from,
                                   bool bumpUnconnecting) override;

    protected:
         CBlockIndex* acceptHeaders(
                const std::vector<CBlockHeader>& headers);

         // private, but protected for unittest
         virtual std::vector<CBlockIndex*> findMissingBlocks(CBlockIndex* last);

    private:
        bool hasEqualOrMoreWork(CBlockIndex* last);
        void suggestDownload(
                const std::vector<CBlockIndex*>& toFetch, CBlockIndex* last);

        CNode* pfrom;
        InFlightIndex& blocksInFlight;
        ThinBlockManager& thinmg;
        BlockInFlightMarker& markAsInFlight;
        std::function<void()> checkBlockIndex;
        std::function<void()> sendGetHeaders;
};

#endif
