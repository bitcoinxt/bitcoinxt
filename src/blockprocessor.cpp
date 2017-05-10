#include "blockprocessor.h"
#include "blockheaderprocessor.h"
#include "consensus/validation.h"
#include "main.h" // Misbehaving
#include "thinblock.h"
#include "util.h"
#include "utilprocessmsg.h"

void BlockProcessor::misbehave(int howmuch) {
    Misbehaving(from.id, howmuch);
}

void BlockProcessor::rejectBlock(
        const uint256& block, const std::string& reason, int misbehave)
{
    LogPrintf("rejecting %s from peer=%d - %s\n",
        netcmd, from.id, reason);

    from.PushMessage("reject", netcmd, REJECT_MALFORMED, reason, block);

    this->misbehave(misbehave);
    worker.setAvailable();
}

bool BlockProcessor::processHeader(const CBlockHeader& header) {

        std::vector<CBlockHeader> h(1, header);

        if (!headerProcessor(h, false, false)) {
            rejectBlock(header.GetHash(), "invalid header", 20);
            return false;
        }
        return true;
}

bool BlockProcessor::setToWork(const uint256& hash) {

    if (HaveBlockData(hash)) {
        LogPrint("thin", "already had block %s, "
                "ignoring %s peer=%d\n",
            hash.ToString(), netcmd, from.id);
        worker.setAvailable();
	    return false;
    }

    if (worker.isAvailable()) {
        // Happens if:
        // * We did not request block.
        // * We already received block (after requesting it) and
        //    found it to be invalid.
        LogPrint("thin", "ignoring %s %s that peer is not working "
                "on peer=%d\n", netcmd, hash.ToString(), from.id);
        return false;
    }

    if (worker.blockHash() != hash) {
        LogPrint("thin", "ignoring %s %s from peer=%d, "
                "expecting block %s\n",
		    netcmd, hash.ToString(), from.id, worker.blockStr());
        return false;
    }
    return true;
}

bool BlockProcessor::requestConnectHeaders(const CBlockHeader& header) {
    bool needPrevHeaders = headerProcessor.requestConnectHeaders(header, from);

    if (needPrevHeaders) {
        // We don't have previous block. We can't connect it to the chain.
        // Ditch it. We will re-request it later if we see that we still want it.
        LogPrint("thin", "Can't connect block %s. We don't have prev. Ignoring it peer=%d.\n",
                header.GetHash().ToString(), from.id);

        worker.setAvailable();
    }
    return needPrevHeaders;
}
