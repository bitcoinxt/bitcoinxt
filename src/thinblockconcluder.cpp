#include "net.h"
#include "thinblockbuilder.h"
#include "thinblockconcluder.h"
#include "util.h"
#include "protocol.h"
#include "chainparams.h"
#include "main.h" // For Misbehaving
#include <vector>

void ThinBlockConcluder::operator()(CNode* pfrom,
    uint64_t nonce, ThinBlockWorker& worker) {

    pfrom->thinBlockNonce = 0;

    // If it the thin block is finished, it the worker will be available.
    if (worker.isAvailable())
        return;

    // If node sends us headers, does not send us a merkleblock, but sends us a pong,
    // it will have a worker without a thin block stub.
    if (!worker.isAvailable() && !worker.isStubBuilt()) {
        LogPrintf("Peer %d did not provide us a merkleblock for %s\n",
                pfrom->id, worker.blockStr());
        misbehaving(pfrom->id, 20);
        worker.setAvailable();
        return;
    }

    if (worker.isReRequesting())
        return giveUp(pfrom, worker);

    reRequest(pfrom, worker, nonce);
}

void ThinBlockConcluder::reRequest(
    CNode* pfrom,
    ThinBlockWorker& worker,
    uint64_t nonce)
{
    std::vector<uint256> txsMissing = worker.getTxsMissing();
    assert(txsMissing.size()); // worker should have been available, not "missing 0 transactions".
    LogPrint("thin", "Missing %d transactions for thin block %s, re-requesting (consider adjusting relay policies)\n",
            txsMissing.size(), worker.blockStr());

    std::vector<CInv> hashesToReRequest;
    typedef std::vector<uint256>::const_iterator auto_;

    for (auto_ m = txsMissing.begin(); m != txsMissing.end(); ++m) {
        hashesToReRequest.push_back(CInv(MSG_TX, *m));
        LogPrint("thin", "Re-requesting tx %s\n", m->ToString());
    }
    assert(hashesToReRequest.size() > 0);
    worker.setReRequesting(true);
    pfrom->thinBlockNonce = nonce;
    pfrom->PushMessage("getdata", hashesToReRequest);
    pfrom->PushMessage("ping", nonce);
}

void ThinBlockConcluder::fallbackDownload(CNode *pfrom, const uint256& block) {
    LogPrint("thin", "Last worker working on %s could not provide missing transactions"
            ", falling back on full block download\n", block.ToString());

    CInv req(MSG_BLOCK, block);
    pfrom->PushMessage("getdata", std::vector<CInv>(1, req));
    markInFlight(pfrom->id, block, Params().GetConsensus(), NULL);
}

void ThinBlockConcluder::giveUp(CNode* pfrom, ThinBlockWorker& worker) {
    LogPrintf("Re-reqested transactions for thin block %s from %d, peer did not follow up.\n",
            worker.blockStr(), pfrom->id);
    uint256 block = worker.blockHash();
    bool wasLastWorker = worker.isOnlyWorker();
    worker.setAvailable();

    // Was this the last peer working on thin block? Fallback to full block download.
    if (wasLastWorker)
        fallbackDownload(pfrom, block);
}


void ThinBlockConcluder::misbehaving(NodeId id, int howmuch) {
    ::Misbehaving(id, howmuch);
}
