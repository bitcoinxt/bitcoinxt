#ifndef BITCOIN_PROCESS_MERKLEBLOCK_H
#define BITCOIN_PROCESS_MERKLEBLOCK_H

class CNode;
class CDataStream;
class ThinBlockWorker;
class TxFinder;

void ProcessMerkleBlock(CNode& pfrom, CDataStream& vRecv,
        ThinBlockWorker& worker,
        const TxFinder& txfinder);

#endif
