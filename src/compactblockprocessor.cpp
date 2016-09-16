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

void CompactBlockProcessor::operator()(CDataStream& vRecv, const CTxMemPool& mempool)
{
    CompactBlock block;
    vRecv >> block;

    const uint256 hash = block.header.GetHash();

    LogPrintf("received compactblock %s from peer=%d\n",
            hash.ToString(), worker.nodeID());

    try {
        validateCompactBlock(block);
    }
    catch (const std::exception& e) {
        LogPrint("thin", "Invalid compact block %s\n", e.what());
        rejectBlock(hash, e.what(), 20);
        return;
    }

    CompactTxFinder txfinder(mempool,
            block.shorttxidk0, block.shorttxidk1);

    processHeader(block.header);

    from.AddInventoryKnown(CInv(MSG_CMPCT_BLOCK, hash));

    if (!setToWork(hash))
        return;

    std::unique_ptr<CompactStub> stub;
    try {
        stub.reset(new CompactStub(block));
        worker.buildStub(*stub, txfinder);
    }
    catch (const thinblock_error& e) {
        rejectBlock(hash, e.what(), 10);
        return;
    }

    // If the stub was enough to finish the block then
    // the worker will be available.
    if (worker.isAvailable())
        return;

    // Request missing
    std::vector<ThinTx> missing = worker.getTxsMissing();
    assert(!missing.empty());

    CompactReRequest req;
    req.blockhash = hash;

    std::vector<ThinTx> all = stub->allTransactions();

    for (auto& tx : missing) {

        auto res = std::find_if(begin(all), end(all), [&tx](const ThinTx& b) {
            return tx.equals(b);
        });
        if (res == end(all)) {
            std::stringstream ss;
            ss << "Error: Did not find " << tx.obfuscated() << " missing, has: ";
            for (auto& a : all)
                ss << a.obfuscated() << "; ";
            throw std::runtime_error(ss.str());
        }

        size_t index = std::distance(begin(all), res);

        req.indexes.push_back(index);
    }

    LogPrint("thin", "re-requesting %d compact txs for %s peer=%d\n",
            req.indexes.size(), hash.ToString(), from.id);
    from.PushMessage("getblocktxn", req);
}
