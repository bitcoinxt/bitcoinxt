#include "blockprocessor.h"
#include "blockheaderprocessor.h"
#include "consensus/validation.h"
#include "main.h" // Misbehaving
#include "thinblock.h"
#include "util.h"
#include "utilprocessmsg.h"

void BlockProcessor::misbehave(int howmuch, const std::string& what) {
    Misbehaving(from.id, howmuch, what);
}

void BlockProcessor::rejectBlock(
        const uint256& block, const std::string& reason, int misbehave)
{
    LogPrintf("rejecting %s from peer=%d - %s\n",
        netcmd, from.id, reason);

    from.PushMessage("reject", netcmd, REJECT_MALFORMED, reason, block);

    this->misbehave(misbehave, "block rejected " + reason);
    worker.stopWork(block);
}

CBlockIndex* BlockProcessor::processHeader(const CBlockHeader& header, bool maybeAnnouncement) {

        std::vector<CBlockHeader> h(1, header);

        try {
            return headerProcessor(h, false, maybeAnnouncement);
        }
        catch (const BlockHeaderError& e) {
            rejectBlock(header.GetHash(), e.what(), 20);
            return nullptr;
        }
}

bool BlockProcessor::setToWork(const CBlockHeader& header, int activeChainHeight) {

    const uint256 hash = header.GetHash();

    if (HaveBlockData(hash)) {
        LogPrint(Log::BLOCK, "already had block %s, "
                "ignoring %s peer=%d\n",
            hash.ToString(), netcmd, from.id);
        worker.stopWork(hash);
	    return false;
    }

    bool isAnnouncement = !worker.isWorkingOn(hash);

    // Don't treat block as announcement, will trigger
    // direct block fetch.
    bool treatAsAnnouncement = false;
    CBlockIndex* index = processHeader(header, treatAsAnnouncement);
    if (!index)
        return false;

    if (isAnnouncement && index->nHeight <= activeChainHeight + 2) {
        // Accept block announcement.
        LogPrint(Log::BLOCK, "received %s %s announcement peer=%d\n",
                netcmd, hash.ToString(), from.id);
        worker.addWork(hash);
        return true;
    }
    else if (isAnnouncement) {
        // Be conservative about block announcements to protect against DoS.
        // We have processed the header, so the block will be re-downloaded
        // later if we really want it.
        LogPrint(Log::BLOCK, "ignoring block announcement %s, height %d is too far "
                "away from active chain %d peer=%d\n", hash.ToString(),
                index->nHeight, activeChainHeight, from.id);
        return false;
    }
    else {
        // this is a block we have requested.
        return true;
    }
}

bool BlockProcessor::requestConnectHeaders(const CBlockHeader& header, bool bumpUnconnecting) {
    bool needPrevHeaders = headerProcessor.requestConnectHeaders(header, from, bumpUnconnecting);

    if (needPrevHeaders) {
        // We don't have previous block. We can't connect it to the chain.
        // Ditch it. We will re-request it later if we see that we still want it.
        LogPrint(Log::BLOCK, "Can't connect block %s. We don't have prev. Ignoring it peer=%d.\n",
                header.GetHash().ToString(), from.id);

        worker.stopWork(header.GetHash());
    }
    return needPrevHeaders;
}
