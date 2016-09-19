// Copyright (c) 2016- The Bitcoin XT developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "xthin.h"
#include "bloom.h"
#include "chainparams.h"
#include "net.h"
#include "pow.h"
#include "protocol.h"
#include <algorithm>

bool isCollision(const std::vector<uint64_t>& txHashes, const uint64_t& hash) {
    return std::find(txHashes.begin(), txHashes.end(), hash)
        != txHashes.end();
}

XThinBlock::XThinBlock() { }

XThinBlock::XThinBlock(const CBlock& block, const CBloomFilter& bloom, bool checkCollision) {
    header = block.GetBlockHeader();

    typedef std::vector<CTransaction>::const_iterator auto_;

    for (auto_ tx = block.vtx.begin(); tx != block.vtx.end(); ++tx) {

        uint64_t hash = tx->GetHash().GetCheapHash();
        if (checkCollision && isCollision(txHashes, hash))
            throw xthin_collision_error();
        txHashes.push_back(hash);

        // Always include coinbase. Coinbase cannot be seen before
        // block is seen.
        if (missing.empty())
        {
            missing.push_back(*tx);
            continue;
        }

        if (!bloom.contains(tx->GetHash()))
            missing.push_back(*tx);
    }
}

void XThinBlock::selfValidate() const {

    if (header.GetHash().IsNull())
        throw std::invalid_argument("xthinblock is Null");

    if (missing.size() == 0)
        throw std::invalid_argument("xthinblock is missing coinbase");

    if (missing.size() > txHashes.size())
        throw std::invalid_argument("xthinblock cannot provide more transactions"
                " than it claims are in block");

    typedef std::vector<uint64_t>::const_iterator auto_;
    std::vector<uint64_t> copy;
    for (auto_ t = txHashes.begin(); t != txHashes.end(); ++t)
    {
        if (isCollision(copy, *t))
            throw std::invalid_argument("hash collision in thin block");
        copy.push_back(*t);
    }

    typedef std::vector<CTransaction>::const_iterator auto__;
    for (auto__ t = missing.begin(); t != missing.end(); ++t) {
        auto_ found = std::find(txHashes.begin(), txHashes.end(), t->GetHash().GetCheapHash());
        if (found != txHashes.end())
            continue;

        throw std::invalid_argument("missing transaction provided "
                " was not listed as belonging to block");
    }
}

// Filter where we tell the node we're requesting a thin block
// from what transactions *not* to include.
CBloomFilter createDontWantFilter(TxHashProvider& hashProvider)
{
    std::vector<uint256> hashes;
    hashProvider(hashes);

    // TODO: Generate a filter in a smart way based on mempool size.
    // From comment for MAX_BLOOM_FILTER_SIZE: 10,000 items and <0.0001%
    int filterElements = std::min(10000, static_cast<int>(hashes.size()));
    int toInsert = filterElements;
    filterElements = std::max(1, filterElements); // >= 1 required by bloom filter
    CBloomFilter filter(filterElements, 0.0001,
            insecure_rand(), BLOOM_UPDATE_ALL);

    for (int i = 0; i < toInsert; ++i)
        filter.insert(hashes[i]);
    return filter;
};

XThinWorker::XThinWorker(ThinBlockManager& m, NodeId n,
                std::unique_ptr<TxHashProvider> h) :
    ThinBlockWorker(m, n),
    HashProvider(h.release())
{
}

// only for unit testing
XThinWorker::XThinWorker(ThinBlockManager& m, NodeId n) : ThinBlockWorker(m, n) {
}

void XThinWorker::requestBlock(const uint256& block,
        std::vector<CInv>& getDataReq, CNode& node)
{
    CBloomFilter dontWant = createDontWantFilter(*HashProvider);

    CInv inv(MSG_XTHINBLOCK, block);
    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
    ss << inv;
    ss << dontWant;
    node.PushMessage("get_xthin", ss);
}


XThinReReqResponse::XThinReReqResponse(const CBlock& srcBlock,
        const std::set<uint64_t>& requesting)
{
    block = srcBlock.GetHash();

    if (requesting.size() == 0)
        throw std::runtime_error("re-requested response with 0 transactions");

    if (srcBlock.vtx.size() < requesting.size())
        throw std::runtime_error("more transactions in re-request than in block");

    typedef std::vector<CTransaction>::const_iterator auto_;
    for (auto_ t = srcBlock.vtx.begin(); t != srcBlock.vtx.end(); ++t)
        if (requesting.count(t->GetHash().GetCheapHash()))
            txRequested.push_back(*t);

    if (txRequested.size() < requesting.size())
        throw std::runtime_error("request contained transactions not in block");
}
