#include "process_merkleblock.h"
#include "main.h"
#include "uint256.h"
#include "net.h"
#include "util.h"
#include "thinblockbuilder.h"
#include "merkleblock.h"
#include "consensus/validation.h"

bool HaveBlockData(const uint256& hash) {
    return mapBlockIndex.count(hash)
        && mapBlockIndex.find(hash)->second->nStatus & BLOCK_HAVE_DATA;
}

// Send a ping to serialize the connection and ensure we can figure out
// when the remote peer thinks it finished sending us data. This reflects
// a minor design weakness in BIP 37 as the merkleblock message does not
// say how many transactions the remote peer thinks we have: this normally
// doesn't matter, but if we have received transactions and then dropped t
// hem due to various policies or running out of memory, then we won't be
// able to reassemble the block. So we must ask the peer to send the
// transactions again.
void SendPing(CNode& pfrom) {
    pfrom.thinBlockNonce = 0;
    while (pfrom.thinBlockNonce == 0)
        GetRandBytes((unsigned char*)&pfrom.thinBlockNonce,
                sizeof(pfrom.thinBlockNonce));

    pfrom.PushMessage("ping", pfrom.thinBlockNonce);
}

void ProcessMerkleBlock(CNode& pfrom, CDataStream& vRecv,
        ThinBlockWorker& worker,
        const TxFinder& txfinder) {

    CMerkleBlock merkleBlock;
    vRecv >> merkleBlock;

    uint256 hash = merkleBlock.header.GetHash();
    pfrom.AddInventoryKnown(CInv(MSG_BLOCK, hash));

    if (HaveBlockData(hash)) {
        LogPrint("thin", "already had block %s, "
            "ignoring merkleblock (peer %d)\n",
            hash.ToString(), pfrom.id);
        worker.setAvailable();
        return;
    }

    if (!worker.isAvailable() && worker.blockHash() != hash)
        LogPrint("thin", "expected peer %d to be working on %s, "
                "but received block %s, switching peer to new block\n",
                pfrom.id, worker.blockStr(), hash.ToString());

    worker.setToWork(hash);

    if (worker.isStubBuilt()) {
        LogPrint("thin", "already built thin block stub "
                "%s (peer %d)\n", hash.ToString(), pfrom.id);
        SendPing(pfrom);
        return;
    }

    LogPrint("thin", "building thin block %s (peer %d) ",
            hash.ToString(), pfrom.id);

    // Now attempt to reconstruct the block from the state of our memory pool.
    // The peer should have already sent us the transactions we need before
    // sending us this message. If it didn't, we just ignore the message
    // entirely for now.
    try {
        worker.buildStub(merkleBlock, txfinder);
        SendPing(pfrom);
    }
    catch (const thinblock_error& e) {
        pfrom.PushMessage("reject", std::string("merkleblock"),
                REJECT_MALFORMED, std::string("bad merkle tree"), hash);
        Misbehaving(pfrom.GetId(), 10);  // FIXME: Is this DoS policy reasonable? Immediate disconnect is better?
        LogPrintf("%s peer=%d", e.what(), pfrom.GetId());
        worker.setAvailable();
        return;
    }
}

