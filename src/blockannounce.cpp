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

bool BlockAnnounceReceiver::isInitialBlockDownload() const {
    return ::IsInitialBlockDownload();
}

bool BlockAnnounceReceiver::fetchAsThin() const {
    if (isInitialBlockDownload())
        return false;

    if (!Opt().UsingThinBlocks())
        return false;

    if (from.SupportsBloomThinBlocks() || from.SupportsXThinBlocks())
        return true;

    if (NodeStatePtr(from.id)->supportsCompactBlocks)
        return true;

    return false;
}

bool BlockAnnounceReceiver::blockHeaderIsKnown() const {
    return mapBlockIndex.count(block);
}

bool BlockAnnounceReceiver::hasBlockData() const {
    return mapBlockIndex.count(block)
        && mapBlockIndex.find(block)->second->nStatus & BLOCK_HAVE_DATA;
}

void BlockAnnounceReceiver::updateBlockAvailability() {
    ::UpdateBlockAvailability(from.id, block);
}

bool BlockAnnounceReceiver::almostSynced() {
    return chainActive.Tip()->GetBlockTime() > GetAdjustedTime() - Params().GetConsensus().nPowTargetSpacing * 20;
}


BlockAnnounceReceiver::DownloadStrategy BlockAnnounceReceiver::pickDownloadStrategy() {

    if (!almostSynced())
        return DONT_DOWNL;

    if (hasBlockData())
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
        LogPrint("thin", "peer %d is busy with %s, won't req %s\n",
                from.id, state->thinblock->blockStr(), block.ToString());
        return DOWNL_THIN_LATER;
    }
    return DOWNL_THIN_NOW;
}

void requestHeaders(CNode& from, const uint256& block) {

    from.PushMessage("getheaders", chainActive.GetLocator(pindexBestHeader), block);
    LogPrint("net", "getheaders (%d) %s to peer=%d\n",
            pindexBestHeader->nHeight, block.ToString(), from.id);
}

// React to a block announcement.
//
// Returns true if we requested the block now from the peer that requested it.
bool BlockAnnounceReceiver::onBlockAnnounced(std::vector<CInv>& toFetch,
        bool announcedAsHeader) {
    AssertLockHeld(cs_main);

    updateBlockAvailability();

    NodeStatePtr nodestate(from.id);
    DownloadStrategy s = pickDownloadStrategy();

    if (s == DOWNL_THIN_NOW) {

        if (!announcedAsHeader && nodestate->prefersHeaders) {
            // Node we're requesting from prefers headers announcement,
            // but sent us an inv announcement.
            //
            // It is likely that we don't have previous blocks,
            // this block may be part of an reorg. So we request headers to
            // be sure that we have all headers leading up to this block.
            requestHeaders(from, block);
        }

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
        requestHeaders(from, block);
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

bool blocksConnect(const std::vector<uint256>& blocksToAnnounce) {

    CBlockIndex* best = nullptr;

    for (auto h : blocksToAnnounce) {
        auto block = mapBlockIndex.find(h)->second;
        if (!best) {
            best = block;
            continue;
        }

        if (block->pprev != best) {
            // This means that the list of blocks to announce don't
            // connect to each other.
            // This shouldn't really be possible to hit during
            // regular operation (because reorgs should take us to
            // a chain that has some block not on the prior chain,
            // which should be caught by the prior check), but one
            // way this could happen is by using invalidateblock /
            // reconsiderblock repeatedly on the tip, causing it to
            // be added multiple times to vBlockHashesToAnnounce.
            // Robustly deal with this rare situation by reverting
            // to an inv.
            LogPrint("ann", "header announce: blocks don't connect: expected %s, is %s\n",
                    best->GetBlockHash().ToString(),
                    block->pprev->GetBlockHash().ToString());
            return false;
        }
        best = block;
    }
    return true;
}

bool BlockAnnounceSender::canAnnounceWithHeaders() const {

    // Set too many blocks to announce. In this corner case
    // we revert to announcing blocks as inv even though
    // peer prefers header announcements.
    if (to.blocksToAnnounce.size() > MAX_BLOCKS_TO_ANNOUNCE)
        return false;

    if (!NodeStatePtr(to.id)->prefersHeaders)
        return false;

    // If we come across any
    // headers that aren't on chainActive, give up.
    for (auto h : to.blocksToAnnounce) {
            auto mi = mapBlockIndex.find(h);
            assert(mi != mapBlockIndex.end());
            auto block = mi->second;
            if (chainActive[block->nHeight] != block)
            {
                LogPrint("ann", "bailing out from header to peer=%d ann, reorged away from %s\n",
                        to.id, block->GetBlockHash().ToString());
                // Bail out if we reorged away from this block
                return false;
            }
    }

    return blocksConnect(to.blocksToAnnounce);
}

bool BlockAnnounceSender::announceWithHeaders() {

    std::vector<CBlock> headers;
    bool foundStartingHeader = false;

    CBlockIndex* best = nullptr;
    for (auto hash : to.blocksToAnnounce) {
        CBlockIndex* block = mapBlockIndex.find(hash)->second;

        if (foundStartingHeader) {
            headers.push_back(block->GetBlockHeader());
            best = block;
            continue;
        }

        if (peerHasHeader(block)) {
            LogPrint("ann", "header ann: peer=%d has header %s\n",
                    to.id, block->GetBlockHash().ToString());
            continue; // keep looking for the first new block
        }

        if (block->pprev == nullptr || peerHasHeader(block->pprev)) {
            // Peer doesn't have this header but they do have the prior one.
            // Start sending headers.
            foundStartingHeader = true;
            headers.push_back(block->GetBlockHeader());
            best = block;
            continue;
        }

        // Peer doesn't have this header or the prior one -- nothing will
        // connect, so bail out.
        LogPrint("ann", "header ann to peer=%d, nothing connects\n",
                to.id);
        return false;
    }

    if (headers.empty())
        return true;

    LogPrint("net", "%s: %u headers, range (%s, %s), to peer=%d\n", __func__,
            headers.size(),
            headers.front().GetHash().ToString(),
            headers.back().GetHash().ToString(), to.id);

    to.PushMessage("headers", headers);
    NodeStatePtr(to.id)->bestHeaderSent = best;

    return true;
}

void BlockAnnounceSender::announce() {
    LOCK(to.cs_inventory);

    if (to.blocksToAnnounce.empty())
        return;

    LogPrint("ann", "Announce for peer=%d, %d blocks, prefersheaders: %d\n",
            to.id, to.blocksToAnnounce.size(), NodeStatePtr(to.id)->prefersHeaders);

    try {
        if (!(canAnnounceWithHeaders() && announceWithHeaders()))
            announceWithInv();
    }
    catch (const std::exception& e) {
        LogPrintf("Error when announcing headers to peer %d: %s\n",
            to.id, e.what());
    }
    to.blocksToAnnounce.clear();
}

void BlockAnnounceSender::announceWithInv() {

    assert(!to.blocksToAnnounce.empty());

    // If falling back to using an inv, just try to inv the tip.
    // The last entry in vBlockHashesToAnnounce was our tip at some point
    // in the past.

    const uint256 &hashToAnnounce = to.blocksToAnnounce.back();
    BlockMap::iterator mi = mapBlockIndex.find(hashToAnnounce);
    assert(mi != mapBlockIndex.end());
    CBlockIndex *pindex = mi->second;

    // If the peer announced this block to us, don't inv it back.
    // (Since block announcements may not be via inv's, we can't solely rely on
    // setInventoryKnown to track this.)
    if (!peerHasHeader(pindex)) {
        to.PushInventory(CInv(MSG_BLOCK, hashToAnnounce));
        LogPrint("net", "%s: sending inv peer=%d hash=%s\n", __func__,
                to.id, hashToAnnounce.ToString());
    }
}

// Requires cs_main
bool BlockAnnounceSender::peerHasHeader(const CBlockIndex *block) const {
    NodeStatePtr state(to.id);
    if (state->pindexBestKnownBlock
        && block == state->pindexBestKnownBlock->GetAncestor(block->nHeight))
        return true;

    if (state->bestHeaderSent
        && block == state->bestHeaderSent->GetAncestor(block->nHeight))
        return true;

    return false;
}

// Find the hashes of all blocks that weren't previously in the best chain.
std::vector<uint256> findHeadersToAnnounce(const CBlockIndex* oldTip, const CBlockIndex* newTip) {

    std::vector<uint256> hashes;
    const CBlockIndex* toAnnounce = newTip;

    while (toAnnounce != oldTip) {
        hashes.push_back(toAnnounce->GetBlockHash());
        toAnnounce = toAnnounce->pprev;

        if (hashes.size() == MAX_BLOCKS_TO_ANNOUNCE) {
            // Limit announcements in case of a huge reorganization.
            // Rely on the peer's synchronization mechanism in that case.
            break;
        }
    }

    // Reverse so that they are in the right order (older block to newest)
    std::reverse(std::begin(hashes), std::end(hashes));
    return hashes;
}
