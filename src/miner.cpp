// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "miner.h"

#include "amount.h"
#include "chainparams.h"
#include "consensus/consensus.h"
#include "consensus/merkle.h"
#include "consensus/tx_verify.h"
#include "consensus/validation.h"
#include "hash.h"
#include "main.h"
#include "maxblocksize.h"
#include "miner/blockbuilder.h"
#include "miner/serializableblockbuilder.h"
#include "miner/utilminer.h"
#include "net.h"
#include "policy/policy.h"
#include "pow.h"
#include "primitives/transaction.h"
#include "timedata.h"
#include "util.h"
#include "utilfork.h"
#include "utilmoneystr.h"
#include "options.h"
#include "validationinterface.h"

#include <boost/thread.hpp>
#include <boost/tuple/tuple.hpp>
#include <stack>
#include <iomanip>
#include <cmath>

using namespace std;

//////////////////////////////////////////////////////////////////////////////
//
// BitcoinMiner
//

//
// Unconfirmed transactions in the memory pool often depend on other
// transactions in the memory pool. When we select transactions from the
// pool, we select by highest or fee rate, so we might consider
// transactions that depend on transactions that aren't yet in the block.

uint64_t nLastBlockTx = 0;
uint64_t nLastBlockSize = 0;

void UpdateTime(CBlockHeader* pblock, const Consensus::Params& consensusParams, const CBlockIndex* pindexPrev)
{
    pblock->nTime = std::max(pindexPrev->GetMedianTimePast()+1, GetAdjustedTime());

    // Updating time can change work required on testnet:
    if (consensusParams.fPowAllowMinDifficultyBlocks) {
        pblock->nBits = GetNextWorkRequired(pindexPrev, pblock->GetBlockTime(), consensusParams);
    }
}

void UpdateTime(miner::BlockBuilder& block,
                const Consensus::Params& consensusParams,
                const CBlockIndex* pindexPrev)
{
    CBlockHeader dummy;
    dummy.nBits = 0;
    UpdateTime(&dummy, consensusParams, pindexPrev);
    block.SetTime(dummy.GetBlockTime());
    if (dummy.nBits != 0) {
        block.SetBits(dummy.nBits);
    }
}

// BIP100 string:
// - Adds our block size vote (B) if configured.
// - Adds Excessive Block (EB) string. This announces how big blocks we currently accept.
std::vector<unsigned char> BIP100Str(uint64_t hardLimit) {
    uint64_t blockVote = Opt().MaxBlockSizeVote();

    std::stringstream ss;
    ss << "/BIP100/";
    if (blockVote)
        ss << "B" << blockVote << "/";
    double dMaxBlockSize = double(hardLimit)/1000000;
    ss << "EB" << std::setprecision(int(log10(dMaxBlockSize))+7) << dMaxBlockSize << "/";

    const std::string s = ss.str();
    return std::vector<unsigned char>(begin(s), end(s));
}

void CreateNewBlock(miner::BlockBuilder& block, const CScript& scriptPubKeyIn, bool checkValidity)
{
    const CChainParams& chainparams = Params();

    // Create coinbase tx
    CMutableTransaction txNew;
    txNew.vin.resize(1);
    txNew.vin[0].prevout.SetNull();
    txNew.vout.resize(1);
    txNew.vout[0].scriptPubKey = scriptPubKeyIn;

    // Largest block you're willing to create:
    const uint64_t hardLimit = GetNextMaxBlockSize(chainActive.Tip(), chainparams.GetConsensus());
    uint64_t nBlockMaxSize = GetArg("-blockmaxsize", hardLimit);
    // Limit to between 1K and (hard limit - 1K) for sanity:
    nBlockMaxSize = std::max((uint64_t)1000, std::min((hardLimit - 1000),  nBlockMaxSize));

    // For compatibility with bip68-sequence test, set flag to not mine txs with negative fee delta.
    const bool fSkipNegativeDelta = GetBoolArg("-bip68hack", false);

    // Collect memory pool transactions into the block
    CTxMemPool::setEntries inBlock;
    CTxMemPool::setEntries gotParents;
    std::stack<CTxMemPool::txiter, std::vector<CTxMemPool::txiter>> clearedTxs;

    uint64_t nBlockSize = 1000;
    uint64_t nBlockTx = 0;
    unsigned int nBlockSigOps = 100;
    int lastFewTxs = 0;
    CAmount nFees = 0;

    {
        LOCK2(cs_main, mempool.cs);
        CBlockIndex* pindexPrev = chainActive.Tip();
        const int nHeight = pindexPrev->nHeight + 1;
        block.SetTime(GetAdjustedTime());
        const int64_t nMedianTimePast = pindexPrev->GetMedianTimePast();
        if (!IsFourthHFActive(nMedianTimePast)) {
            block.DisableLTOR();
        }

        // -regtest only: allow overriding block.nVersion with
        // -blockversion=N to test forking scenarios
        block.SetVersion(ComputeBlockVersion(pindexPrev, chainparams.GetConsensus()));
        if (Params().MineBlocksOnDemand()) {
            block.SetVersion(GetArg("-blockversion", block.GetVersion()));
        }

        int64_t nLockTimeCutoff = (STANDARD_LOCKTIME_VERIFY_FLAGS & LOCKTIME_MEDIAN_TIME_PAST)
                                ? nMedianTimePast
                                : block.GetTime();

        CTxMemPool::indexed_transaction_set::nth_index<3>::type::iterator mi = mempool.mapTx.get<3>().begin();
        CTxMemPool::txiter iter;

        while (mi != mempool.mapTx.get<3>().end() || !clearedTxs.empty())
        {
            if (clearedTxs.empty()) { // add tx with next highest score
                iter = mempool.mapTx.project<0>(mi);
                mi++;
            }
            else {  // try to add a previously cleared tx
                iter = clearedTxs.top();
                clearedTxs.pop();
            }

            if (inBlock.count(iter)) {
                continue;
            }

            miner::BuilderEntry tx(&(iter->GetTx()), iter->GetSigOpCount(), iter->GetFee());

            if (fSkipNegativeDelta && mempool.GetFeeModifier().GetDelta(tx.GetHash()) < 0) {
                continue;
            }

            // Our index guarantees that all ancestors are paid for.
            // If it has parents, push this tx, then its parents, onto the stack.
            // The second time we process a tx, just make sure all parents are in the block
            bool fAllParentsInBlock = true;
            bool fPushedAParent = false;
            bool fFirstTime = !gotParents.count(iter);
            gotParents.insert(iter);
            BOOST_FOREACH(CTxMemPool::txiter parent, mempool.GetMemPoolParents(iter))
            {
                if (!inBlock.count(parent)) {
                    fAllParentsInBlock = false;
                    if (fFirstTime) {
                        if (!fPushedAParent) {
                            clearedTxs.push(iter);
                            fPushedAParent = true;
                        }
                        clearedTxs.push(parent);
                    }
                }
            }
            if (fPushedAParent || !fAllParentsInBlock) {
                continue;
            }

            unsigned int nTxSize = iter->GetTxSize();
            if (nBlockSize + nTxSize >= nBlockMaxSize) {
                if (nBlockSize >  nBlockMaxSize - 100 || lastFewTxs > 50) {
                    break;
                }
                // Once we're within 1000 bytes of a full block, only look at 50 more txs
                // to try to fill the remaining space.
                if (nBlockSize > nBlockMaxSize - 1000) {
                    lastFewTxs++;
                }
                continue;
            }

            if (!tx.IsFinalTx(nHeight, nLockTimeCutoff))
                continue;

            // TODO: with more complexity we could make the block bigger when
            // sigop-constrained and sigop density in later megabytes is low
            const uint32_t& nTxSigOps = tx.GetSigOpCount();
            if (nBlockSigOps + nTxSigOps >= MaxBlockSigops(nBlockSize)) {
                if (nBlockSigOps > MaxBlockSigops(nBlockSize) - 2) {
                    break;
                }
                continue;
            }

            // Added
            block.AddTx(std::move(tx));
            nBlockSize += nTxSize;
            ++nBlockTx;
            nBlockSigOps += nTxSigOps;
            nFees += tx.GetFee();

            inBlock.insert(iter);
        }

        nLastBlockTx = nBlockTx;
        nLastBlockSize = nBlockSize;
        LogPrintf("CreateNewBlock(): total size %u txs: %u fees: %ld sigops %d\n", nBlockSize, nBlockTx, nFees, nBlockSigOps);

        // Compute final coinbase transaction.
        txNew.vout[0].nValue = nFees + GetBlockSubsidy(nHeight, chainparams.GetConsensus());
        txNew.vin[0].scriptSig = CScript() << nHeight << BIP100Str(hardLimit) << OP_0;
        miner::BloatCoinbaseSize(txNew);
        CTransaction coinbaseTx(txNew);
        miner::BuilderEntry coinbase(&coinbaseTx, GetLegacySigOpCount(txNew, STANDARD_CHECKDATASIG_VERIFY_FLAGS), 0);
        block.SetCoinbase(std::move(coinbase));

        // Fill in header
        block.SetHashPrevBlock(pindexPrev->GetBlockHash());
        UpdateTime(block, Params().GetConsensus(), pindexPrev);
        block.SetBits(GetNextWorkRequired(pindexPrev, block.GetTime(), Params().GetConsensus()));

        if (block.NeedsGBTMetadata()) {
            block.SetBlockMinTime(nMedianTimePast + 1);
            block.SetBlockHeight(nHeight);
            block.SetBlockSizeLimit(hardLimit);
            block.SetBlockSigopLimit(MaxBlockSigops(nBlockSize));

            CScript flags = CScript() << BIP100Str(hardLimit);
            flags +=  COINBASE_FLAGS;
            block.SetCoinbaseAuxFlags(std::move(flags));
            std::map<Consensus::DeploymentPos, ThresholdState> bip135state;
            for (int i = 0; i < static_cast<int>(Consensus::MAX_VERSION_BITS_DEPLOYMENTS); ++i) {
                Consensus::DeploymentPos pos = Consensus::DeploymentPos(i);
                if (!IsConfiguredDeployment(chainparams.GetConsensus(), pos)) {
                    continue;
                }
                bip135state[pos] = VersionBitsState(pindexPrev,
                                                     chainparams.GetConsensus(),
                                                     pos, versionbitscache);
            }
            block.SetBIP135State(bip135state);
        }
        block.Finalize(chainparams.GetConsensus());

        if (checkValidity) {
            block.CheckValidity(pindexPrev);
        }
    }
}

void IncrementExtraNonce(CBlock* pblock, CBlockIndex* pindexPrev, unsigned int& nExtraNonce, uint64_t nMaxBlockSize)
{
    // Update nExtraNonce
    static uint256 hashPrevBlock;
    if (hashPrevBlock != pblock->hashPrevBlock)
    {
        nExtraNonce = 0;
        hashPrevBlock = pblock->hashPrevBlock;
    }
    ++nExtraNonce;
    unsigned int nHeight = pindexPrev->nHeight+1; // Height first in coinbase required for block.version=2
    CMutableTransaction txCoinbase(pblock->vtx[0]);
    txCoinbase.vin[0].scriptSig = (CScript() << nHeight << BIP100Str(nMaxBlockSize) << CScriptNum(nExtraNonce)) + COINBASE_FLAGS;
    assert(txCoinbase.vin[0].scriptSig.size() <= 100);

    pblock->vtx[0] = txCoinbase;
    pblock->hashMerkleRoot = BlockMerkleRoot(*pblock);
}

//////////////////////////////////////////////////////////////////////////////
//
// Internal miner
//

//
// ScanHash scans nonces looking for a hash with at least some zero bits.
// The nonce is usually preserved between calls, but periodically or if the
// nonce is 0xffff0000 or above, the block is rebuilt and nNonce starts over at
// zero.
//
bool static ScanHash(const CBlockHeader *pblock, uint32_t& nNonce, uint256 *phash)
{
    // Write the first 76 bytes of the block header to a double-SHA256 state.
    CHash256 hasher;
    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
    ss << *pblock;
    assert(ss.size() == 80);
    hasher.Write((unsigned char*)&ss[0], 76);

    while (true) {
        nNonce++;

        // Write the last 4 bytes of the block header (the nonce) to a copy of
        // the double-SHA256 state, and compute the result.
        CHash256(hasher).Write((unsigned char*)&nNonce, 4).Finalize((unsigned char*)phash);

        // Return the nonce if the hash has at least some zero bits,
        // caller will check if it has enough to reach the target
        if (((uint16_t*)phash)[15] == 0)
            return true;

        // If nothing found after trying for a while, return -1
        if ((nNonce & 0xfff) == 0)
            return false;
    }
}

static bool ProcessBlockFound(CBlock* pblock, const CChainParams& chainparams, CConnman* connman)
{
    LogPrintf("%s\n", pblock->ToString());
    LogPrintf("generated %s\n", FormatMoney(pblock->vtx[0].vout[0].nValue));

    // Found a solution
    {
        LOCK(cs_main);
        if (pblock->hashPrevBlock != chainActive.Tip()->GetBlockHash())
            return error("BitcoinMiner: generated block is stale");
    }

    // Inform about the new block
    GetMainSignals().BlockFound(pblock->GetHash());

    // Process this block the same as if we had received it from another node
    CValidationState state;
    if (!ProcessNewBlock(state, BlockSource{}, pblock, true, nullptr, connman))
        return error("BitcoinMiner: ProcessNewBlock, block not accepted");

    return true;
}

void static BitcoinMiner(const CChainParams& chainparams, CConnman* connman)
{
    LogPrintf("BitcoinMiner started\n");
    SetThreadPriority(THREAD_PRIORITY_LOWEST);
    RenameThread("bitcoin-miner");

    unsigned int nExtraNonce = 0;

    boost::shared_ptr<CReserveScript> coinbaseScript;
    GetMainSignals().ScriptForMining(coinbaseScript);

    try {
        // Throw an error if no script was provided.  This can happen
        // due to some internal error but also if the keypool is empty.
        // In the latter case, already the pointer is NULL.
        if (!coinbaseScript || coinbaseScript->reserveScript.empty())
            throw std::runtime_error("No coinbase script available (mining requires a wallet)");

        while (true) {
            if (chainparams.MiningRequiresPeers()) {
                // Busy-wait for the network to come online so we don't waste time mining
                // on an obsolete chain. In regtest mode we expect to fly solo.
                do {
                    bool fvNodesEmpty = bool(connman->GetNodeCount(CConnman::CONNECTIONS_ALL));
                    if (!fvNodesEmpty && !IsInitialBlockDownload())
                        break;
                    MilliSleep(1000);
                } while (true);
            }

            //
            // Create new block
            //
            unsigned int nTransactionsUpdatedLast = mempool.GetTransactionsUpdated();
            CBlockIndex* pindexPrev = chainActive.Tip();

            CBlock block;
            {
                miner::SerializableBlockBuilder blockbuilder;
                CreateNewBlock(blockbuilder, coinbaseScript->reserveScript);
                block = blockbuilder.Release();
            }
            uint64_t hardLimit = GetNextMaxBlockSize(pindexPrev, chainparams.GetConsensus());
            IncrementExtraNonce(&block, pindexPrev, nExtraNonce, hardLimit);

            LogPrintf("Running BitcoinMiner with %u transactions in block (%u bytes)\n", block.vtx.size(),
                ::GetSerializeSize(block, SER_NETWORK, PROTOCOL_VERSION));

            //
            // Search
            //
            int64_t nStart = GetTime();
            arith_uint256 hashTarget = arith_uint256().SetCompact(block.nBits);
            uint256 hash;
            uint32_t nNonce = 0;
            while (true) {
                // Check if something found
                if (ScanHash(&block, nNonce, &hash))
                {
                    if (UintToArith256(hash) <= hashTarget)
                    {
                        // Found a solution
                        block.nNonce = nNonce;
                        assert(hash == block.GetHash());

                        SetThreadPriority(THREAD_PRIORITY_NORMAL);
                        LogPrintf("BitcoinMiner:\n");
                        LogPrintf("proof-of-work found  \n  hash: %s  \ntarget: %s\n", hash.GetHex(), hashTarget.GetHex());
                        ProcessBlockFound(&block, chainparams, connman);
                        SetThreadPriority(THREAD_PRIORITY_LOWEST);
                        coinbaseScript->KeepScript();

                        // In regression test mode, stop mining after a block is found.
                        if (chainparams.MineBlocksOnDemand())
                            throw boost::thread_interrupted();

                        break;
                    }
                }

                // Check for stop or if block needs to be rebuilt
                boost::this_thread::interruption_point();
                // Regtest mode doesn't require peers
                bool connected = bool(connman->GetNodeCount(CConnman::CONNECTIONS_ALL));
                if (!connected && chainparams.MiningRequiresPeers())
                    break;
                if (nNonce >= 0xffff0000)
                    break;
                if (mempool.GetTransactionsUpdated() != nTransactionsUpdatedLast && GetTime() - nStart > 60)
                    break;
                if (pindexPrev != chainActive.Tip())
                    break;

                UpdateTime(&block, chainparams.GetConsensus(), pindexPrev);
                if (chainparams.GetConsensus().fPowAllowMinDifficultyBlocks)
                {
                    // Changing pblock->nTime can change work required on testnet:
                    hashTarget.SetCompact(block.nBits);
                }
            }
        }
    }
    catch (const boost::thread_interrupted&)
    {
        LogPrintf("BitcoinMiner terminated\n");
        throw;
    }
    catch (const std::runtime_error &e)
    {
        LogPrintf("BitcoinMiner runtime error: %s\n", e.what());
        return;
    }
}

void GenerateBitcoins(bool fGenerate, int nThreads, const CChainParams& chainparams, CConnman* connman)
{
    static boost::thread_group* minerThreads = NULL;

    if (nThreads < 0)
        nThreads = GetNumCores();

    if (minerThreads != NULL)
    {
        minerThreads->interrupt_all();
        delete minerThreads;
        minerThreads = NULL;
    }

    if (nThreads == 0 || !fGenerate)
        return;

    minerThreads = new boost::thread_group();
    for (int i = 0; i < nThreads; i++)
        minerThreads->create_thread(boost::bind(&BitcoinMiner, boost::cref(chainparams), connman));
}
