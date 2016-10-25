#include "process_merkleblock.h"
#include "blockheaderprocessor.h"
#include "bloomthin.h"
#include "consensus/validation.h"
#include "main.h" // Misbehaving, mapBlockIndex
#include "merkleblock.h"
#include "net.h"
#include "thinblock.h"
#include "uint256.h"
#include "util.h"
#include "utilprocessmsg.h"

// Send a ping to serialize the connection and ensure we can figure out
// when the remote peer thinks it finished sending us data. This reflects
// a minor design weakness in BIP 37 as the merkleblock message does not
// say how many transactions the remote peer thinks we have: this normally
// doesn't matter, but if we have received transactions and then dropped t
// hem due to various policies or running out of memory, then we won't be
// able to reassemble the block. So we must ask the peer to send the
// transactions again.
void SendPing(CNode& pfrom, const uint256& block) {
    pfrom.thinBlockNonceBlock = block;
    pfrom.thinBlockNonce = 0;
    while (pfrom.thinBlockNonce == 0)
        GetRandBytes((unsigned char*)&pfrom.thinBlockNonce,
                sizeof(pfrom.thinBlockNonce));

    pfrom.PushMessage("ping", pfrom.thinBlockNonce);
}

void ProcessMerkleBlock(CNode& pfrom, CDataStream& vRecv,
        ThinBlockWorker& worker,
        const TxFinder& txfinder,
        BlockHeaderProcessor& processHeader)
{

    CMerkleBlock merkleBlock;
    vRecv >> merkleBlock;

    if (pfrom.SupportsXThinBlocks()) {
        LogPrint("thin", "Ignoring merkleblock from peer=%d,"
                "peer should send xthin blocks\n", pfrom.id);
        return;
    }

    uint256 hash = merkleBlock.header.GetHash();
    pfrom.AddInventoryKnown(CInv(MSG_BLOCK, hash));

    if (HaveBlockData(hash)) {
        LogPrint("thin", "already had block %s, "
                "ignoring merkleblock (peer %d)\n",
                hash.ToString(), pfrom.id);
        worker.setAvailable();
        return;
    }

    std::vector<CBlockHeader> headers(1, merkleBlock.header);
    if (!processHeader(headers, false, false)) {
        LogPrint("thin", "Header failed for merkleblock peer=%d\n", pfrom.id);
        worker.setAvailable();
        return;
    }

    if (!worker.isAvailable() && worker.blockHash() != hash)
        LogPrint("thin", "expected peer %d to be working on %s, "
                "but received block %s, switching peer to new block\n",
                pfrom.id, worker.blockStr(), hash.ToString());

    worker.setToWork(hash);

    LogPrint("thin", "received stub for block %s (peer %d) ",
            hash.ToString(), pfrom.id);

    // Now attempt to reconstruct the block from the state of our memory pool.
    try {
        ThinBloomStub stubData(merkleBlock);
        worker.buildStub(stubData, txfinder);
        SendPing(pfrom, hash);
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

