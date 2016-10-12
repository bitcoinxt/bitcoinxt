#ifndef BITCOIN_BLOCKHEDERPROCESSOR_H

#include <vector>
#include <functional>
#include <tuple>

class CNode;
class CBlockHeader;
class CBlockIndex;
class InFlightIndex;
class ThinBlockManager;
class BlockInFlightMarker;

class BlockHeaderProcessor {
    public:
        virtual bool operator()(const std::vector<CBlockHeader>& headers,
                bool peerSentMax,
                bool maybeAnnouncement) = 0;
        virtual ~BlockHeaderProcessor() = 0;
};
inline BlockHeaderProcessor::~BlockHeaderProcessor() { }

class DefaultHeaderProcessor : public BlockHeaderProcessor {
    public:

        DefaultHeaderProcessor(CNode* pfrom,
                InFlightIndex&, ThinBlockManager&, BlockInFlightMarker&,
                std::function<void()> checkBlockIndex,
                std::function<void()> sendGetHeaders = [](){ });

        bool operator()(const std::vector<CBlockHeader>& headers,
                bool peerSentMax,
                bool maybeAnnouncement) override;

    protected:
        virtual std::tuple<bool, CBlockIndex*> acceptHeaders(
                const std::vector<CBlockHeader>& headers);

    private:

        std::vector<CBlockIndex*> findMissingBlocks(CBlockIndex* last);

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
