#include "blockencodings.h"
#include "net.h"
#include "thinblockbuilder.h"
#include "thinblockconcluder.h"
#include "util.h"
#include "protocol.h"
#include "chainparams.h"
#include "main.h" // For Misbehaving
#include "xthin.h"
#include "utilprocessmsg.h"
#include <vector>
#include "compactthin.h"

static void fallbackDownload(CNode& from,
        const uint256& block, BlockInFlightMarker& markInFlight) {

    LogPrint("thin", "Falling back on full block download for %s peer=%d\n",
            block.ToString(), from.id);

    CInv req(MSG_BLOCK, block);
    from.PushMessage("getdata", std::vector<CInv>(1, req));
    markInFlight(from.id, block, Params().GetConsensus(), NULL);
}

static void handleReRequestFailed(ThinBlockWorker& worker, CNode& from, uint256 block,
        BlockInFlightMarker& markInFlight) {

    LogPrint("thin", "Did not provide all missing transactions in a"
            "thin block re-request for %s peer=%d\n",
            block.ToString(), worker.nodeID());

    bool wasLastWorker = worker.isOnlyWorker(block);
    worker.stopWork(block);
    if (wasLastWorker) {
        // Node deserves misbehave, but we'll give it a chance
        // to send us the full block.
        fallbackDownload(from, block, markInFlight);
    }
    else
        Misbehaving(worker.nodeID(), 10);
}

void XThinBlockConcluder::operator()(const XThinReReqResponse& resp,
        CNode& pfrom, ThinBlockWorker& worker, BlockInFlightMarker& markInFlight) {

    if (!worker.isWorkingOn(resp.block))
    {
        LogPrint("thin", "got xthin re-req response for %s, but not "
                "working on block peer=%d\n", resp.block.ToString(), pfrom.id);
        return;
    }

    for (auto& t : resp.txRequested)
        worker.addTx(resp.block, t);

    // Block fully constructed?
    if (worker.isWorkingOn(resp.block))
        handleReRequestFailed(worker, pfrom, resp.block, markInFlight);
}

void CompactBlockConcluder::operator()(const CompactReReqResponse& resp,
        CNode& pfrom, ThinBlockWorker& worker, BlockInFlightMarker& markInFlight) {

    if (!worker.isWorkingOn(resp.blockhash))
    {
        LogPrint("thin", "got compact re-req response for %s, but not "
                "working on block peer=%d\n", resp.blockhash.ToString(), pfrom.id);
        return;
    }

    for (auto& t : resp.txn)
        worker.addTx(resp.blockhash, t);

    // Block fully constructed?
    if (worker.isWorkingOn(resp.blockhash))
        handleReRequestFailed(worker, pfrom, resp.blockhash, markInFlight);
}
