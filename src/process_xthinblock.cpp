#include "process_xthinblock.h"
#include "blockheaderprocessor.h"
#include "consensus/validation.h"
#include "main.h" // Misbehaving, mapBlockIndex
#include "thinblock.h"
#include "util.h"
#include "utilprocessmsg.h"
#include "xthin.h"
#include "thinblock.h"


void XThinBlockProcessor::operator()(
        CDataStream& vRecv, ThinBlockWorker& worker, const TxFinder& txfinder,
        BlockHeaderProcessor& processHeader)

{
    XThinBlock block;
    vRecv >> block;

    uint256 hash = block.header.GetHash();

    LogPrintf("received xthinblock %s from peer=%d\n",
            hash.ToString(), worker.nodeID());

    try {
        block.selfValidate();
        if (!processHeader(std::vector<CBlockHeader>(1, block.header), false, false))
            throw std::invalid_argument("invalid header");
    }
    catch (const std::invalid_argument& e) {
        pfrom.PushMessage("reject", std::string("xthinblock"),
                REJECT_MALFORMED, std::string(e.what()), hash);
        worker.setAvailable();
        misbehave(20);
        return;
    }

    pfrom.AddInventoryKnown(CInv(MSG_XTHINBLOCK, hash));

    if (HaveBlockData(hash)) {
        LogPrint("thin", "already had block %s, "
                "ignoring xthinblock peer=%d\n",
            hash.ToString(), pfrom.id);
        worker.setAvailable();
	return;
    }

    if (worker.isAvailable()) {
        // Happens if:
        // * We did not request block.
        // * We already received block (after requesting it) and
        //    found it to be invalid.
        LogPrint("thin", "ignoring xthinblock %s that peer is not working "
                "on peer=%d\n", block.header.GetHash().ToString(), pfrom.id);
        return;
    }

    if (worker.blockHash() != hash) {
        LogPrint("thin", "ignoring xthinblock %s from peer=%d, "
                "expecting block %s\n",
		hash.ToString(), pfrom.id, worker.blockStr());
        return;
    }

    try {
        XThinStub stub(block);
        worker.buildStub(stub, txfinder);
    }
    catch (const thinblock_error& e) {
        pfrom.PushMessage("reject", std::string("xthinblock"),
                REJECT_MALFORMED, std::string(e.what()), hash);
        misbehave(10);
        worker.setAvailable();
        return;
    }

    // If the stub was enough to finish the block then
    // the worker will be available.
    if (worker.isAvailable())
        return;

    // Request missing
    std::vector<ThinTx> missing = worker.getTxsMissing();
    assert(!missing.empty());

    XThinReRequest req;
    req.block = hash;
    typedef std::vector<ThinTx>::const_iterator auto_;
    for (auto_ t = missing.begin(); t != missing.end(); ++t)
        req.txRequesting.insert(t->cheap());

    LogPrintf("re-requesting %d missing transactions for %s from peer=%d\n",
            missing.size(), hash.ToString(), pfrom.id);
    pfrom.PushMessage("get_xblocktx", req);
}

void XThinBlockProcessor::misbehave(int howmuch) {
    Misbehaving(pfrom.id, howmuch);
}
