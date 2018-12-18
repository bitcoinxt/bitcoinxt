#ifndef BITCOIN_GBTBLOCKBUILDER_H
#define BITCOIN_GBTBLOCKBUILDER_H

#include "miner/blockbuilder.h"
#include <univalue.h>

namespace miner {

class GBTBlockBuilder : public BlockBuilder {

public:
    GBTBlockBuilder();

    void SetTime(uint32_t t) override;
    uint32_t GetTime() const override;
    void SetVersion(uint32_t v) override;
    uint32_t GetVersion() const override;
    void SetCoinbase(BuilderEntry tx) override;
    void AddTx(BuilderEntry tx) override;
    void SetBits(uint32_t bits) override;
    uint32_t GetBits();
    void SetHashPrevBlock(const uint256& hash) override;
    void DisableLTOR() override;
    void Finalize(const Consensus::Params& consensusParams) override;
    void CheckValidity(CBlockIndex* pindexPrev) override;

    // GBT specific
    virtual bool NeedsGBTMetadata() const { return true; }

    void SetBlockMinTime(int64_t t) override;
    void SetBlockHeight(int32_t h) override;
    void SetBIP135State(const std::map<Consensus::DeploymentPos,
                        ThresholdState>& state) override;

    void SetBlockSizeLimit(uint64_t limit) override;
    void SetBlockSigopLimit(uint64_t limit) override;
    void SetCoinbaseAuxFlags(CScript flags) override;

    void SetClientRules(std::set<std::string> clientRules);
    void SetMaxVersionPreVB(int64_t v);
    void SetLongPollID(const uint256& tip, uint32_t mempoolTxUpdated);

    UniValue Release() {
        return std::move(block);
    }

private:
    UniValue block;
    CBlockHeader dummyheader;
    BuilderEntry coinbase;
    std::vector<BuilderEntry> txs;
    std::map<Consensus::DeploymentPos, ThresholdState> bip135state;
    bool useLTOR;
    std::set<std::string> clientRules;
    int64_t nMaxVersionPreVB;
    CScript coinbaseaux;
    std::string longpollid;
    int32_t blockHeight;
    int64_t blockMinTime;
    uint64_t blockMaxSize;
    uint64_t blockMaxSigops;
};

} // ns miner

#endif
