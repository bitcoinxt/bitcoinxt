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


void BloomBlockConcluder::operator()(CNode* pfrom,
    uint64_t nonce, ThinBlockWorker& worker) {

    // If it the thin block is finished, it the worker will be available.
    if (worker.isAvailable()) {
        pfrom->thinBlockNonce = 0;
        pfrom->thinBlockNonceBlock.SetNull();
        return;
    }

    if (pfrom->thinBlockNonceBlock != worker.blockHash()) {
        LogPrint("thin", "thinblockconcluder: pong response for a different download (%s, currently waiting for %s). Ignoring.\n",
            pfrom->thinBlockNonceBlock.ToString(), worker.blockStr());
        return;
    }

    pfrom->thinBlockNonce = 0;
    pfrom->thinBlockNonceBlock.SetNull();

    // If node sends us headers, does not send us a merkleblock, but sends us a pong,
    // then the worker will be without a stub.
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

void BloomBlockConcluder::reRequest(
    CNode* pfrom,
    ThinBlockWorker& worker,
    uint64_t nonce)
{
    std::vector<ThinTx> txsMissing = worker.getTxsMissing();
    assert(txsMissing.size()); // worker should have been available, not "missing 0 transactions".
    LogPrint("thin", "Missing %d transactions for thin block %s, re-requesting (consider adjusting relay policies)\n",
            txsMissing.size(), worker.blockStr());

    std::vector<CInv> hashesToReRequest;
    typedef std::vector<ThinTx>::const_iterator auto_;

    for (auto_ m = txsMissing.begin(); m != txsMissing.end(); ++m) {
        hashesToReRequest.push_back(CInv(MSG_TX, m->full()));
        LogPrint("thin", "Re-requesting tx %s\n", m->full().ToString());
    }
    assert(hashesToReRequest.size() > 0);
    worker.setReRequesting(true);
    pfrom->thinBlockNonce = nonce;
    pfrom->thinBlockNonceBlock = worker.blockHash();
    pfrom->PushMessage("getdata", hashesToReRequest);
    pfrom->PushMessage("ping", nonce);
}

void BloomBlockConcluder::fallbackDownload(CNode *pfrom, const uint256& block) {
    LogPrint("thin", "Last worker working on %s could not provide missing transactions"
            ", falling back on full block download\n", block.ToString());

    CInv req(MSG_BLOCK, block);
    pfrom->PushMessage("getdata", std::vector<CInv>(1, req));
    markInFlight(pfrom->id, block, Params().GetConsensus(), NULL);
}

void BloomBlockConcluder::giveUp(CNode* pfrom, ThinBlockWorker& worker) {
    LogPrintf("Re-requested transactions for thin block %s from %d, peer did not follow up.\n",
            worker.blockStr(), pfrom->id);
    uint256 block = worker.blockHash();
    bool wasLastWorker = worker.isOnlyWorker();
    worker.setAvailable();

    // Was this the last peer working on thin block? Fallback to full block download.
    if (wasLastWorker)
        fallbackDownload(pfrom, block);
}


void BloomBlockConcluder::misbehaving(NodeId id, int howmuch) {
    ::Misbehaving(id, howmuch);
}

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
