#include "blockheaderprocessor.h"
#include "blockannounce.h"
#include "chain.h"
#include "consensus/validation.h"
#include "main.h" // Misbehaving, UpdateBlockAvailability
#include "net.h"
#include "primitives/block.h"
#include "util.h"
#include "inflightindex.h"
#include "thinblockconcluder.h" // BlockInFlightMarker
#include <boost/range/adaptor/reversed.hpp>

using namespace std;

DefaultHeaderProcessor::DefaultHeaderProcessor(CNode* pfrom,
        InFlightIndex& i,
        ThinBlockManager& mg,
        BlockInFlightMarker& inFlight,
        std::function<void()> checkBlockIndex) :
    pfrom(pfrom), blocksInFlight(i), thinmg(mg),
    markAsInFlight(inFlight), checkBlockIndex(checkBlockIndex) { }

// maybeAnnouncement: Header *might* have been received as a block announcement.
bool DefaultHeaderProcessor::operator()(const std::vector<CBlockHeader>& headers,
        bool peerSentMax,
        bool maybeAnnouncement)
{
    CBlockIndex *pindexLast = nullptr;
    for (const CBlockHeader& header : headers) {
        CValidationState state;
        if (pindexLast != nullptr && header.hashPrevBlock != pindexLast->GetBlockHash()) {
            Misbehaving(pfrom->GetId(), 20);
            return error("non-continuous headers sequence");
        }
        if (!AcceptBlockHeader(header, state, &pindexLast)) {
            int nDoS;
            if (state.IsInvalid(nDoS)) {
                if (nDoS > 0)
                    Misbehaving(pfrom->GetId(), nDoS);
                return error("invalid header received");
            }
        }
    }

    if (pindexLast)
        UpdateBlockAvailability(pfrom->GetId(), pindexLast->GetBlockHash());

    if (peerSentMax && pindexLast) {
        // Headers message had its maximum size; the peer may have more headers.
        // TODO: optimize: if pindexLast is an ancestor of chainActive.Tip or pindexBestHeader, continue
        // from there instead.
        LogPrint("net", "more getheaders (%d) to end to peer=%d (startheight:%d)\n", pindexLast->nHeight, pfrom->id, pfrom->nStartingHeight);
        pfrom->PushMessage("getheaders", chainActive.GetLocator(pindexLast), uint256());
    }

    if (pindexLast && maybeAnnouncement) {
        std::vector<CBlockIndex*> toFetch = findMissingBlocks(pindexLast);

        // We may or may not start downloading the blocks
        // from this peer now.
        suggestDownload(toFetch, pindexLast);
    }

    checkBlockIndex();
    return true;
}

std::vector<CBlockIndex*> DefaultHeaderProcessor::findMissingBlocks(CBlockIndex* last) {
    assert(last);

    vector<CBlockIndex*> toFetch;
    CBlockIndex* walk = last;

    // Calculate all the blocks we'd need to switch to last, up to a limit.
    do {
        if (toFetch.size() == MAX_BLOCKS_IN_TRANSIT_PER_PEER)
            break;

        if (chainActive.Contains(walk))
            break;

        if (walk->nStatus & BLOCK_HAVE_DATA)
            continue;

        if (blocksInFlight.isInFlight(walk->GetBlockHash()))
            continue;

        // We don't have this block, and it's not yet in flight.
        toFetch.push_back(walk);

    } while ((walk = walk->pprev));

    return toFetch;
}

bool DefaultHeaderProcessor::hasEqualOrMoreWork(CBlockIndex* last) {
    return last->IsValid(BLOCK_VALID_TREE)
        && chainActive.Tip()->nChainWork <= last->nChainWork;
}

void DefaultHeaderProcessor::suggestDownload(const std::vector<CBlockIndex*>& toFetch, CBlockIndex* last) {
    std::vector<CInv> toGet;

    if (!hasEqualOrMoreWork(last))
        return;

    for (auto b : boost::adaptors::reverse(toFetch)) {

        BlockAnnounceReceiver ann(b->GetBlockHash(),
                *pfrom, thinmg, blocksInFlight);

        // Stop if we don't want to download this block now.
        // Won't want next.
        if (!ann.onBlockAnnounced(toGet, true))
            break;

        // This block has been requested from peer.
        markAsInFlight(pfrom->id, b->GetBlockHash(), Params().GetConsensus(), nullptr);
    }

    if (!toGet.empty()) {
        LogPrint("net", "Downloading blocks toward %s (%d) via headers direct fetch\n",
                last->GetBlockHash().ToString(), last->nHeight);
        pfrom->PushMessage("getdata", toGet);
    }
}
