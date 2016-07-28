// Copyright (c) 2016 The Bitcoin XT developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#include "blockannounce.h"
#include "main.h" // for mapBlockIndex, UpdateBlockAvailability, IsInitialBlockDownload
#include "options.h"
#include "timedata.h"
#include "inflightindex.h"
#include "nodestate.h"
#include "util.h"
#include "thinblockmanager.h"
#include "thinblock.h"

bool BlockAnnounceProcessor::isInitialBlockDownload() const {
    return ::IsInitialBlockDownload();
}

bool BlockAnnounceProcessor::fetchAsThin() const {
    return !isInitialBlockDownload() && Opt().UsingThinBlocks()
        && (from.SupportsBloomThinBlocks() || from.SupportsXThinBlocks());
}

bool BlockAnnounceProcessor::wantBlock() const {
    return !mapBlockIndex.count(block);
}

void BlockAnnounceProcessor::updateBlockAvailability() {
    ::UpdateBlockAvailability(from.id, block);
}

bool BlockAnnounceProcessor::almostSynced() {
    return chainActive.Tip()->GetBlockTime() > GetAdjustedTime() - Params().GetConsensus().nPowTargetSpacing * 20;
}

        
BlockAnnounceProcessor::DownloadStrategy BlockAnnounceProcessor::pickDownloadStrategy() {

    if (!almostSynced())
        return DONT_DOWNL;

    if (!fetchAsThin()) {
    
        if (blocksInFlight.isInFlight(block))
            return DONT_DOWNL;

        NodeStatePtr nodestate(from.id);
        if (nodestate->nBlocksInFlight >= MAX_BLOCKS_IN_TRANSIT_PER_PEER)
            return DONT_DOWNL;

        if (Opt().AvoidFullBlocks()) {
            LogPrint("thin", "avoiding full blocks, not requesting %s from %d\n",
                block.ToString(), from.id);

            return DONT_DOWNL;
        }
        
        return DOWNL_FULL_NOW;
    }

    // At this point we know we want a thin block.

    NodeStatePtr state(from.id);

    if (!state->initialHeadersReceived)
        return DOWNL_THIN_LATER;

    if (thinmg.numWorkers(block) >= Opt().ThinBlocksMaxParallel()) {
        LogPrint("thin", "max parallel thin req reached, not req %s from peer %d\n",
                block.ToString(), from.id);
        return DONT_DOWNL;
    }

    if (!state->thinblock->isAvailable()) {
        LogPrint("thin", "peer %d is busy, won't req %s\n",
                from.id, block.ToString());
        return DOWNL_THIN_LATER;
    }

    return DOWNL_THIN_NOW;
}

// React to a block announcement.
//
// Returns true if we requested the block now from the peer that requested it.
bool BlockAnnounceProcessor::onBlockAnnounced(std::vector<CInv>& toFetch) {
    AssertLockHeld(cs_main);

    updateBlockAvailability();

    if (!wantBlock())
        return false;

    NodeStatePtr nodestate(from.id);
    DownloadStrategy s = pickDownloadStrategy();
    
    if (s == DOWNL_THIN_NOW) {
        int numDownloading = thinmg.numWorkers(block);
        LogPrint("thin", "requesting %s from peer %d (%d of %d parallel)\n",
            block.ToString(), from.id, (numDownloading + 1), Opt().ThinBlocksMaxParallel());

        nodestate->thinblock->requestBlock(block, toFetch, from);
        nodestate->thinblock->setToWork(block);
        return true;
    }

    // First request the headers preceding the announced block. In the normal fully-synced
    // case where a new block is announced that succeeds the current tip (no reorganization),
    // there are no such headers.
    // Secondly, and only when we are close to being synced, we request the announced block directly,
    // to avoid an extra round-trip. Note that we must *first* ask for the headers, so by the
    // time the block arrives, the header chain leading up to it is already validated. Not
    // doing this will result in the received block being rejected as an orphan in case it is
    // not a direct successor.

    if (!blocksInFlight.isInFlight(block) || !nodestate->initialHeadersReceived) {
        from.PushMessage("getheaders", chainActive.GetLocator(pindexBestHeader), block);
        LogPrint("net", "getheaders (%d) %s to peer=%d\n",
                pindexBestHeader->nHeight, block.ToString(), from.id);
    }
    
    if (s == DONT_DOWNL)
        return false;

    if (s == DOWNL_THIN_LATER)
        return false;
    
    assert(s == DOWNL_FULL_NOW);

    LogPrint("thin", "full block download of %s from %d\n",
            block.ToString(), from.id);
    toFetch.push_back(CInv(MSG_BLOCK, block));
    return true;
}
