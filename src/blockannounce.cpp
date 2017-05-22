// Copyright (c) 2016-2017 The Bitcoin XT developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#include "blockannounce.h"
#include "blocksender.h"
#include "main.h" // for mapBlockIndex, UpdateBlockAvailability, IsInitialBlockDownload
#include "options.h"
#include "timedata.h"
#include "inflightindex.h"
#include "nodestate.h"
#include "util.h"
#include "utilprocessmsg.h"
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

    if (from.SupportsXThinBlocks() || NodeStatePtr(from.id)->supportsCompactBlocks)
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

    if (thinmg.numWorkers(block) >= Opt().ThinBlocksMaxParallel()) {
        LogPrint("thin", "max parallel thin req reached, not req %s from peer %d\n",
                block.ToString(), from.id);
        return DONT_DOWNL;
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
bool BlockAnnounceReceiver::onBlockAnnounced(std::vector<CInv>& toFetch) {
    AssertLockHeld(cs_main);

    updateBlockAvailability();

    NodeStatePtr nodestate(from.id);
    DownloadStrategy s = pickDownloadStrategy();

    if (s == DOWNL_THIN_NOW) {
        int numDownloading = thinmg.numWorkers(block);
        LogPrint("thin", "requesting %s from peer %d (%d of %d parallel)\n",
            block.ToString(), from.id, (numDownloading + 1), Opt().ThinBlocksMaxParallel());

        nodestate->thinblock->requestBlock(block, toFetch, from);
        nodestate->thinblock->addWork(block);
        return true;
    }

    // Request headers preceding the announced block (if any), so by the time
    // the full block arrives, the header chain leading up to it is already
    // validated. Not doing this will result in block being discarded and
    // re-downloaded later.
    if (!hasBlockData() && !blocksInFlight.isInFlight(block))
        requestHeaders(from, block);

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

// We only announce with a thin block if there is one block to announce.
// In addition, same limitations as announcing as header apply.
bool BlockAnnounceSender::canAnnounceWithBlock() const {

    if(to.blocksToAnnounce.size() != 1)
        return false;

    if (!canAnnounceWithHeaders())
        return false;

    uint256 hash = to.blocksToAnnounce[0];
    CBlockIndex* block = mapBlockIndex.find(hash)->second;
    return block->nStatus & BLOCK_HAVE_DATA;
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
    UpdateBestHeaderSent(to, best);

    return true;
}

void BlockAnnounceSender::announceWithBlock(BlockSender& sender) {
    assert(to.blocksToAnnounce.size() == 1);
    uint256 hash = to.blocksToAnnounce[0];
    CBlockIndex* block = mapBlockIndex.find(hash)->second;

    if (peerHasHeader(block)) {
        // peer may have announced this block to us.
        return;
    }

    sender.sendBlock(to, *block, MSG_CMPCT_BLOCK, block->nHeight);
    UpdateBestHeaderSent(to, block);

    LogPrint("net", "%s: block %s to peer=%d\n",
            __func__, hash.ToString(), to.id);
}

void BlockAnnounceSender::announce() {
    LOCK(to.cs_inventory);

    if (to.blocksToAnnounce.empty())
        return;

    NodeStatePtr node(to.id);
    LogPrint("ann", "Announce for peer=%d, %d blocks, prefersheaders: %d, prefersblocks: %d\n",
            to.id, to.blocksToAnnounce.size(),
            node->prefersHeaders, node->prefersBlocks);

    try {
        bool announced = false;

        if (node->prefersBlocks && canAnnounceWithBlock()) {
            BlockSender sender;
            announceWithBlock(sender);
            announced = true;
        }

        else if (node->prefersHeaders && canAnnounceWithHeaders())
            announced = announceWithHeaders();

        if (!announced)
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
