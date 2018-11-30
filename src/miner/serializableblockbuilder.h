#ifndef BITCOIN_MINER_SERIALIZABLEBLOCKBUILDER
#define BITCOIN_MINER_SERIALIZABLEBLOCKBUILDER

#include "miner/blockbuilder.h"

namespace miner {

// This interface creates a 'CBlock', which is serializes to the correct Bitcoin
// format.
class SerializableBlockBuilder : public BlockBuilder {
public:
    SerializableBlockBuilder();

    void SetTime(uint32_t t) override;
    uint32_t GetTime() const override;
    void SetVersion(uint32_t v) override;
    uint32_t GetVersion() const override;
    void SetCoinbase(BuilderEntry tx) override;
    void AddTx(BuilderEntry tx) override;
    void SetBits(uint32_t bits) override;
    void SetHashPrevBlock(const uint256& hash) override;
    void DisableLTOR() override;
    void Finalize(const Consensus::Params&) override;
    void CheckValidity(CBlockIndex* pindexPrev) override;

    // GBT stuff. We don't need it.
    bool NeedsGBTMetadata() const override {
        return false;
    }

    void SetBlockMinTime(int64_t) override { }
    void SetBlockHeight(int32_t) override { }
    void SetBIP135State(const std::map<Consensus::DeploymentPos, ThresholdState>&) override { }
    void SetBlockSizeLimit(uint64_t) override { }
    void SetBlockSigopLimit(uint64_t) override { }
    void SetCoinbaseAuxFlags(CScript) override { }

    // Takes ownership. Any operations on SerializableBlockBuilder (this) are
    // undefined after this is called.
    CBlock Release();

private:
    bool useLTOR;
    bool isFinalized;
    BuilderEntry coinbase;
    std::vector<BuilderEntry> txs;
    CBlock block;
};

} // ns miner

#endif
