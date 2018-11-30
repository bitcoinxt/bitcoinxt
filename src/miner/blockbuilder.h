#ifndef BITCOIN_MINER_BLOCKBUILDER
#define BITCOIN_MINER_BLOCKBUILDER

#include "consensus/tx_verify.h"
#include "versionbits.h" // ThresholdState

namespace miner {

class BuilderEntry {
public:

    BuilderEntry(const CTransaction* t, uint32_t sigopcount, const CAmount& fee)
        : tx(t), sigOpCount(sigopcount), nFee(fee) {
    }

    const uint256& GetHash() const {
        assert(IsValid());
        return tx->GetHash();
    }

    bool IsFinalTx(const int nHeight, int64_t nLockTimeCutoff) const {
        assert(IsValid());
        return ::IsFinalTx(*tx, nHeight, nLockTimeCutoff);
    }

    const CTransaction& GetTx() const {
        assert(IsValid());
        return *tx;
    }

    const uint32_t& GetSigOpCount() const {
        return sigOpCount;
    }

    const CAmount& GetFee() const {
        return nFee;
    }

    bool IsCoinBase() const {
        assert(IsValid());
        return tx->IsCoinBase();
    }

    bool IsValid() const {
        return tx != nullptr;
    }

private:
    const CTransaction* tx;
    uint32_t sigOpCount;
    CAmount nFee;
};

inline bool EntryHashCmp(const BuilderEntry& a, const BuilderEntry& b) {
    return a.GetHash() < b.GetHash();
}

class BlockBuilder {
public:
    virtual void SetTime(uint32_t t) = 0;
    virtual uint32_t GetTime() const = 0;

    virtual void SetVersion(uint32_t v) = 0;
    virtual uint32_t GetVersion() const = 0;

    virtual void SetCoinbase(BuilderEntry tx) = 0;
    virtual void AddTx(BuilderEntry tx) = 0;

    virtual void SetBits(uint32_t bits) = 0;
    virtual void SetHashPrevBlock(const uint256& hash) = 0;

    // If LTOR is disabled, it becomes the callers responsiblity to add
    // transactions in the correct order.
    virtual void DisableLTOR() = 0;

    // As BlockBuilder only holds references to transactions, this needs to be
    // called before any of them go out of scope.
    virtual void Finalize(const Consensus::Params&) = 0;

    // Sanity check.
    // Validates the block contents and throws if it's not valid.
    //
    // This check is optional and can be slow, as
    // a CBlock needs to be constructed.
    virtual void CheckValidity(CBlockIndex* pindexPrev) = 0;

    // If the interface requires some or all of the extended metadata required
    // by 'getblocktemplate' (aka GBT).
    //
    // If this functions return false, calling the GBT functions is optional.
    virtual bool NeedsGBTMetadata() const = 0;

    // Below are methods for feeding GBT with data. If NeedsGBTMetadata is true,
    // these must be called.
    virtual void SetBlockMinTime(int64_t) = 0;
    virtual void SetBlockHeight(int32_t) = 0;
    virtual void SetBIP135State(const std::map<Consensus::DeploymentPos, ThresholdState>& state) = 0;
    virtual void SetBlockSizeLimit(uint64_t limit) = 0;
    virtual void SetBlockSigopLimit(uint64_t limit) = 0;
    virtual void SetCoinbaseAuxFlags(CScript) = 0;

};

} // ns miner

#endif
