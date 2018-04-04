#include "compactblockprocessor.h"
#include "compactthin.h"
#include "blockencodings.h"
#include "thinblock.h"
#include "streams.h"
#include "util.h" // LogPrintf
#include "net.h"
#include "utilprocessmsg.h"
#include "consensus/validation.h"
#include "compacttxfinder.h"

void CompactBlockProcessor::operator()(CDataStream& vRecv, const CTxMemPool& mempool,
        uint64_t currMaxBlockSize, int activeChainHeight)
{
    CompactBlock block;
    vRecv >> block;

    const uint256 hash = block.header.GetHash();

    LogPrintf("received compactblock %s from peer=%d\n",
            hash.ToString(), worker.nodeID());

    try {
        validateCompactBlock(block, currMaxBlockSize);
    }
    catch (const std::exception& e) {
        LogPrint(Log::BLOCK, "Invalid compact block %s\n", e.what());
        rejectBlock(hash, e.what(), 20);
        return;
    }

    // We (may) have requested node to announce blocks to us and it will
    // continue to do so even if we haven't kept up. Don't misbehave it for
    // sending us unconnected headers.
    bool bumpUnconnecting = false;
    if (requestConnectHeaders(block.header, bumpUnconnecting)) {
        worker.stopWork(hash);
        return;
    }

    if (!setToWork(block.header, activeChainHeight))
        return;

    from.AddInventoryKnown(CInv(MSG_CMPCT_BLOCK, hash));

    std::unique_ptr<CompactStub> stub;
    try {
        CompactTxFinder txfinder(mempool,
                block.shorttxidk0, block.shorttxidk1);

        stub.reset(new CompactStub(block));
        worker.buildStub(*stub, txfinder, from);
    }
    catch (const thinblock_error& e) {
        rejectBlock(hash, e.what(), 10);
        return;
    }

    if (!worker.isWorkingOn(hash)) {
        // Stub had enough data to finish
        // the block.
        return;
    }

    // Request missing
    std::vector<std::pair<int, ThinTx> > missing = worker.getTxsMissing(hash);

    CompactReRequest req;
    req.blockhash = hash;

    for (auto& t : missing)
        req.indexes.push_back(t.first /* index in block */);

    LogPrint(Log::BLOCK, "re-requesting %d compact txs for %s peer=%d\n",
            req.indexes.size(), hash.ToString(), from.id);
    from.PushMessage("getblocktxn", req);
}
