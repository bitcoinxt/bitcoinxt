#include "miner/gbtblockbuilder.h"

#include "consensus/validation.h"
#include "core_io.h" // EncodeHexTx
#include "main.h" // TestBlockValidity
#include "miner/gbtparser.h"
#include "utildebug.h"
#include "utilhash.h"
#include "utilstrencodings.h"

#include <unordered_map>

static const Consensus::ForkDeployment& gbt_vb_fork(
        const Consensus::Params& consensusParams,
        const Consensus::DeploymentPos pos)
{
    return consensusParams.vDeployments.at(pos);
}

static const std::string gbt_vb_name(const Consensus::ForkDeployment& fork) {
    std::string s = fork.name;
    if (!fork.gbt_force) {
        s.insert(s.begin(), '!');
    }
    return s;
}

namespace miner {

GBTBlockBuilder::GBTBlockBuilder() : block(UniValue::VOBJ), coinbase(nullptr, 0, 0),
                                     useLTOR(true), nMaxVersionPreVB(-1), blockHeight(-1),
                                     blockMinTime(-1), blockMaxSize(0), blockMaxSigops(0)
{
}

void GBTBlockBuilder::SetTime(uint32_t t) {
    dummyheader.nTime = t;
}

uint32_t GBTBlockBuilder::GetTime() const {
    return dummyheader.GetBlockTime();
}

void GBTBlockBuilder::SetVersion(uint32_t v) {
    dummyheader.nVersion = v;
}

uint32_t GBTBlockBuilder::GetVersion() const {
    return dummyheader.nVersion;
}

void GBTBlockBuilder::SetCoinbase(BuilderEntry tx) {
    coinbase = std::move(tx);
}

void GBTBlockBuilder::AddTx(BuilderEntry tx) {
    txs.emplace_back(std::move(tx));
}

void GBTBlockBuilder::SetBits(uint32_t bits) {
    dummyheader.nBits = bits;
}

uint32_t GBTBlockBuilder::GetBits() {
    return dummyheader.nBits;
}

void GBTBlockBuilder::SetHashPrevBlock(const uint256& hash) {
    dummyheader.hashPrevBlock = hash;
}

// If LTOR is disabled, it becomes the callers responsiblity to add
// transactions in the correct order.
void GBTBlockBuilder::DisableLTOR() {
    useLTOR = false;
}

void GBTBlockBuilder::Finalize(const Consensus::Params& consensusParams) {
    THROW_UNLESS(block.empty());
    THROW_UNLESS(coinbase.IsCoinBase());
    THROW_UNLESS(blockMinTime != -1);
    THROW_UNLESS(blockHeight != -1);
    THROW_UNLESS(blockMaxSize != 0);
    THROW_UNLESS(blockMaxSigops != 0);
    THROW_UNLESS(!longpollid.empty());

    if (useLTOR) {
        std::sort(std::begin(txs), std::end(txs), EntryHashCmp);
    }

    UniValue aCaps(UniValue::VARR);
    aCaps.push_back("proposal");
    UniValue transactions(UniValue::VARR);

    std::unordered_map<uint256, int64_t, SaltedTxIDHasher> setTxIndex;
    int64_t i = 0;
    for (const auto& tx : txs) {
        const uint256& txHash = tx.GetHash();
        setTxIndex[txHash] = i++;

        THROW_UNLESS(!tx.IsCoinBase());

        UniValue entry(UniValue::VOBJ);
        entry.push_back(Pair("data", EncodeHexTx(tx.GetTx())));
        entry.push_back(Pair("hash", txHash.GetHex()));

        UniValue deps(UniValue::VARR);
        for (const CTxIn &in : tx.GetTx().vin) {
            if (setTxIndex.count(in.prevout.hash)) {
                deps.push_back(setTxIndex[in.prevout.hash]);
            }
        }
        entry.push_back(Pair("depends", deps));
        entry.push_back(Pair("fee", tx.GetFee()));
        entry.push_back(Pair("sigops", uint64_t(tx.GetSigOpCount())));
        transactions.push_back(entry);
    }

    arith_uint256 hashTarget = arith_uint256().SetCompact(GetBits());

    UniValue aMutable(UniValue::VARR);
    aMutable.push_back("time");
    aMutable.push_back("transactions");
    aMutable.push_back("prevblock");

    block.push_back(Pair("capabilities", aCaps));

    UniValue aRules(UniValue::VARR);
    UniValue vbavailable(UniValue::VOBJ);
    for (auto& bit_threshold : bip135state) {
        Consensus::DeploymentPos bit = bit_threshold.first;
        ThresholdState state = bit_threshold.second;

        switch (state) {
        case THRESHOLD_DEFINED:
        case THRESHOLD_FAILED:
            // Not exposed to GBT at all
            break;
        case THRESHOLD_LOCKED_IN:
            // Ensure bit is set in block version
            dummyheader.nVersion |= VersionBitsMask(consensusParams, bit);
            // FALLTHROUGH
            // to get vbavailable set...
        case THRESHOLD_STARTED:
            {
                const Consensus::ForkDeployment &fork = gbt_vb_fork(consensusParams, bit);
                std::string forkName = gbt_vb_name(fork);
                vbavailable.push_back(Pair(forkName, bit));
                if (clientRules.find(fork.name) == clientRules.end())
                {
                    if (!fork.gbt_force)
                    {
                        // If the client doesn't support this, don't indicate it
                        // in the [default] version
                        dummyheader.nVersion &= ~VersionBitsMask(consensusParams, bit);
                    }
                }
                break;
            }
        case THRESHOLD_ACTIVE:
            {
                // Add to rules only
                const Consensus::ForkDeployment &fork = gbt_vb_fork(consensusParams, bit);
                std::string forkName = gbt_vb_name(fork);
                aRules.push_back(forkName);
                if (clientRules.find(fork.name) == clientRules.end()) {
                    // Not supported by the client; make sure it's safe to
                    // proceed
                    if (!fork.gbt_force) {
                        // If we do anything other than throw an exception here,
                        // be sure version/force isn't sent to old clients
                        throw std::invalid_argument(
                                strprintf("Support for '%s' rule requires explicit client support", fork.name));
                    }
                }
                break;
            }
        }
    }
    block.push_back(Pair("version", dummyheader.nVersion));
    block.push_back(Pair("rules", aRules));
    block.push_back(Pair("vbavailable", vbavailable));
    block.push_back(Pair("vbrequired", int(0)));

    if (nMaxVersionPreVB >= 2) {
        // If VB is supported by the client, nMaxVersionPreVB is -1, so we
        // won't get here Because BIP 34 changed how the generation
        // transaction is serialised, we can only use version/force back to
        // v2 blocks This is safe to do [otherwise-]unconditionally only
        // because we are throwing an exception above if a non-force
        // deployment gets activated Note that this can probably also be
        // removed entirely after the first BIP9/BIP135 non-force deployment
        // (ie, segwit) gets activated
        aMutable.push_back("version/force");
    }

    UniValue aux(UniValue::VOBJ);
    aux.push_back(Pair("flags", HexStr(coinbaseaux.begin(), coinbaseaux.end())));

    block.push_back(Pair("previousblockhash", dummyheader.hashPrevBlock.GetHex()));
    block.push_back(Pair("transactions", transactions));
    block.push_back(Pair("coinbaseaux", aux));
    block.push_back(Pair("coinbasevalue", coinbase.GetFee()));
    block.push_back(Pair("longpollid", longpollid));
    block.push_back(Pair("target", hashTarget.GetHex()));
    block.push_back(Pair("mintime", blockMinTime));
    block.push_back(Pair("mutable", aMutable));
    block.push_back(Pair("noncerange", "00000000ffffffff"));
    block.push_back(Pair("sigoplimit", blockMaxSigops));
    block.push_back(Pair("sizelimit", blockMaxSize));
    block.push_back(Pair("curtime", int64_t(GetTime())));
    block.push_back(Pair("bits", strprintf("%08x", GetBits())));
    block.push_back(Pair("height", blockHeight));
}

void GBTBlockBuilder::CheckValidity(CBlockIndex* pindexPrev) {
    CValidationState state;
    CBlock b = ParseGBT(block);
    if (!TestBlockValidity(state, b, pindexPrev, false, false)) {
        std::stringstream err;
        err << __func__ << ": TestBlockValidity failed: "
            << FormatStateMessage(state);
        throw std::runtime_error(err.str());
    }
}

void GBTBlockBuilder::SetBlockMinTime(int64_t t) {
    blockMinTime = t;
}

void GBTBlockBuilder::SetBlockHeight(int32_t h) {
    blockHeight = h;
}

void GBTBlockBuilder::SetBIP135State(const std::map<Consensus::DeploymentPos,
                                     ThresholdState>& state) {
    bip135state = state;
}
void GBTBlockBuilder::SetBlockSizeLimit(uint64_t limit) {
    blockMaxSize = limit;
}

void GBTBlockBuilder::SetBlockSigopLimit(uint64_t limit) {
    blockMaxSigops = limit;
}

void GBTBlockBuilder::SetCoinbaseAuxFlags(CScript flags) {
    coinbaseaux = std::move(flags);
}

void GBTBlockBuilder::SetClientRules(std::set<std::string> clientRules) {
    this->clientRules = std::move(clientRules);
}

void GBTBlockBuilder::SetMaxVersionPreVB(int64_t v) {
    nMaxVersionPreVB = v;
}

void GBTBlockBuilder::SetLongPollID(const uint256& tip, uint32_t mempoolTxUpdated) {
    std::stringstream ss;
    ss << tip.GetHex() << mempoolTxUpdated;
    longpollid = ss.str();
}

} // ns miner
