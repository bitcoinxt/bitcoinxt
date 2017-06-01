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

void XThinBlockConcluder::operator()(const XThinReReqResponse& resp,
        CNode& pfrom, ThinBlockWorker& worker) {

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

    // Block finished?
    if (worker.isAvailable())
        return;

    // There is no reason for remote peer not to have provided all
    // transactions at this point.
    LogPrint("thin", "peer=%d responded to re-request for block %s, "
        "but still did not provide all transctions missing\n",
        pfrom.id, resp.block.ToString());

    worker.setAvailable();
    Misbehaving(pfrom.id, 10);
}

void CompactBlockConcluder::operator()(const CompactReReqResponse& resp,
        CNode& pfrom, ThinBlockWorker& worker) {

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

    // Block finished?
    if (worker.isAvailable())
        return;

    // There is no reason for remote peer not to have provided all
    // transactions at this point.
    LogPrint("thin", "peer=%d responded to compact re-request for block %s, "
        "but still did not provide all transctions missing\n",
        pfrom.id, resp.blockhash.ToString());

    worker.setAvailable();
    Misbehaving(pfrom.id, 10);
}
