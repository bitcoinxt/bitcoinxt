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

static void handleReRequestFailed(ThinBlockWorker& worker, CNode& from,
        BlockInFlightMarker& markInFlight) {

    LogPrint("thin", "Did not provide all missing transactions in a"
            "thin block re-request for %s peer=%d\n",
            worker.blockStr(), worker.nodeID());

    bool wasLastWorker = worker.isOnlyWorker();
    uint256 wasWorkingOn = worker.blockHash();
    worker.setAvailable();
    if (wasLastWorker) {
        // Node deserves misbehave, but we'll give it a chance
        // to send us the full block.
        fallbackDownload(from, wasWorkingOn, markInFlight);
    }
    else
        Misbehaving(worker.nodeID(), 10);
}

void XThinBlockConcluder::operator()(const XThinReReqResponse& resp,
        CNode& pfrom, ThinBlockWorker& worker, BlockInFlightMarker& markInFlight) {

    if (worker.isAvailable())
    {
        LogPrint("thin", "worker peer=%d should not be working on a xthin block,"
                "ignoring re-req response\n", pfrom.id);
        return;
    }
    if (worker.blockHash() != resp.block) {
        LogPrint("thin", "working on block %s, got xthin re-req response from peer=%d for %s\n",
                worker.blockStr(), pfrom.id, resp.block.ToString());
        return;
    }

    typedef std::vector<CTransaction>::const_iterator auto_;
    for (auto_ t = resp.txRequested.begin(); t != resp.txRequested.end(); ++t)
        worker.addTx(*t);

    // Block fully constructed?
    if (!worker.isAvailable())
        handleReRequestFailed(worker, pfrom, markInFlight);
}

void CompactBlockConcluder::operator()(const CompactReReqResponse& resp,
        CNode& pfrom, ThinBlockWorker& worker, BlockInFlightMarker& markInFlight) {

    if (worker.isAvailable())
    {
        LogPrint("thin", "worker peer=%d should not be working on a compact thin block,"
                "ignoring re-req response\n", pfrom.id);
        return;
    }
    if (worker.blockHash() != resp.blockhash) {
        LogPrint("thin", "working on block %s, got compact re-req response from peer=%d for %s\n",
                worker.blockStr(), pfrom.id, resp.blockhash.ToString());
        return;
    }

    for (auto& t : resp.txn)
        worker.addTx(t);

    // Block fully constructed?
    if (!worker.isAvailable())
        handleReRequestFailed(worker, pfrom, markInFlight);
}
