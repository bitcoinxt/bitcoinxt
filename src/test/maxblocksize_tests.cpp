// Copyright (c) 2017 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "maxblocksize.h"
#include "chain.h"
#include "chainparams.h"
#include "options.h"
#include "test/test_bitcoin.h"
#include "test/testutil.h"

#include <boost/test/unit_test.hpp>

#include <algorithm>

BOOST_FIXTURE_TEST_SUITE(maxblocksize_tests, BasicTestingSetup)

void fillBlockIndex(
        Consensus::Params& params, std::vector<CBlockIndex>& blockIndexes,
        bool addVotes, int64_t currMax) {

    auto customizeFunc = [addVotes, currMax](CBlockIndex& index) {
        index.nMaxBlockSize = currMax;

        if (!addVotes) return;

        index.nMaxBlockSizeVote = std::max(index.nHeight * 1000000, 1000000);
    };
    BuildDummyBlockIndex(blockIndexes, customizeFunc, Opt().UAHFTime());
};

BOOST_AUTO_TEST_CASE(get_next_max_blocksize) {
    auto params = Params(CBaseChainParams::REGTEST).GetConsensus();
    BOOST_CHECK_EQUAL(uint32_t(1512), params.nMaxBlockSizeChangePosition);

    // Genesis block, legacy block size
    BOOST_CHECK_EQUAL(MAX_BLOCK_SIZE, GetNextMaxBlockSize(nullptr, params));

    const int64_t interval = params.DifficultyAdjustmentInterval();

    // Not at a difficulty adjustment interval,
    // should not change max block size.
    {
        uint64_t currMax = UAHF_INITIAL_MAX_BLOCK_SIZE;
        std::vector<CBlockIndex> blockInterval(interval);
        fillBlockIndex(params, blockInterval, true, currMax);
        CBlockIndex index;

        for (auto& b : blockInterval) {
            if ((b.nHeight + 1) % interval == 0)
                continue;
            BOOST_CHECK_EQUAL(currMax,
                GetNextMaxBlockSize(&b, params));

        }
    }

    // No block voted. Keep current size.
    {
        uint64_t currMax = 2000000;
        std::vector<CBlockIndex> blockInterval(interval);
        fillBlockIndex(params, blockInterval, false, currMax);

        BOOST_CHECK_EQUAL(currMax,
                GetNextMaxBlockSize(&blockInterval.back(), params));
    }

    // Everyone votes current size. Keep current size.
    {
        uint64_t currMax = 2000000;
        std::vector<CBlockIndex> blockInterval(interval);
        fillBlockIndex(params, blockInterval, false, currMax);

        for (CBlockIndex& b : blockInterval)
            b.nMaxBlockSizeVote = currMax;

        BOOST_CHECK_EQUAL(currMax,
                GetNextMaxBlockSize(&blockInterval.back(), params));
    }

    // Everyone votes.
    // Blocks vote (vote# * 1MB)
    {
        // Test raise.
        uint64_t currMax = 2000000;
        std::vector<CBlockIndex> blockInterval(interval);
        fillBlockIndex(params, blockInterval, true, currMax);
        uint64_t newLimit = GetNextMaxBlockSize(&blockInterval.back(), params);
        BOOST_CHECK_EQUAL(uint64_t(currMax * 1.05), newLimit);

        // Test lower.
        currMax = 1000 * 2000000;
        fillBlockIndex(params, blockInterval, true, currMax);
        newLimit = GetNextMaxBlockSize(&blockInterval.back(), params);
        BOOST_CHECK_EQUAL(uint64_t(currMax / 1.05), newLimit);
    }
}

std::vector<unsigned char> to_uchar(const std::string& coinbaseStr) {
    return std::vector<unsigned char>(begin(coinbaseStr), end(coinbaseStr));
}

// If we have an explicit /B/ vote, we read it and ignore /EB/.
BOOST_AUTO_TEST_CASE(get_max_blocksize_vote_b) {

    std::vector<unsigned char> vote(to_uchar("/BIP100/B2/EB1/"));
    int32_t height = 600000;

    // Coinbase as in the internal miner
    CScript coinbase = CScript() << height << vote << OP_0;
    BOOST_CHECK_EQUAL(2000000u, GetMaxBlockSizeVote(coinbase, height));

    // Coinbase as created with IncrementExtraNonce
    unsigned int nonce = 1;
    CScript coinbase_flags;
    coinbase = (CScript() << height << vote << CScriptNum(nonce)) + coinbase_flags;
    BOOST_CHECK_EQUAL(2000000u, GetMaxBlockSizeVote(coinbase, height));

    // coinbase without height should also work
    coinbase = (CScript() << vote << CScriptNum(nonce)) + coinbase_flags;
    BOOST_CHECK_EQUAL(2000000u, GetMaxBlockSizeVote(coinbase, height));

    // can't vote twice, only first one counts.
    coinbase = (CScript() << to_uchar("/BIP100/B4/EB6/BIP100/B8/"));
    BOOST_CHECK_EQUAL(4000000u, GetMaxBlockSizeVote(coinbase, height));

    // B-votes override EB, even though EB is first.
    coinbase = (CScript() << to_uchar("/EB6/BIP100/B8/"));
    BOOST_CHECK_EQUAL(8000000u, GetMaxBlockSizeVote(coinbase, height));
}

// If /B/ is not present, we count /EB/ as a vote.
BOOST_AUTO_TEST_CASE(get_max_blocksize_vote_eb) {
    std::vector<unsigned char> vote(to_uchar("/some data/EB1/"));
    int32_t height = 600000;

    CScript coinbase = CScript() << height << vote << OP_0;
    BOOST_CHECK_EQUAL(1000000u, GetMaxBlockSizeVote(coinbase, height));

    unsigned int nonce = 1;
    CScript coinbase_flags;
    coinbase = (CScript() << height << vote << CScriptNum(nonce)) + coinbase_flags;
    BOOST_CHECK_EQUAL(1000000u, GetMaxBlockSizeVote(coinbase, height));

    // Example of a Bitcoin Unlimited coinbase string
    coinbase = CScript() << height << to_uchar("/EB16/AD12/a miner comment");
    BOOST_CHECK_EQUAL(16000000u, GetMaxBlockSizeVote(coinbase, height));

    // can't vote twice, only first one counts.
    coinbase = (CScript() << to_uchar("some data/EB6/EB8/"));
    BOOST_CHECK_EQUAL(6000000u, GetMaxBlockSizeVote(coinbase, height));
}

BOOST_AUTO_TEST_CASE(get_max_blocksize_vote_no_vote) {
    int32_t height = 600000;
    BOOST_CHECK_EQUAL(0u, GetMaxBlockSizeVote(CScript() << height << OP_0, height));

    // votes must begin and end with /
    BOOST_CHECK_EQUAL(0u, GetMaxBlockSizeVote(CScript() << height << to_uchar("/EB2"), height));
    BOOST_CHECK_EQUAL(0u, GetMaxBlockSizeVote(CScript() << height << to_uchar("EB2/"), height));
    BOOST_CHECK_EQUAL(0u, GetMaxBlockSizeVote(CScript() << height << to_uchar("/BIP100/B2"), height));
    BOOST_CHECK_EQUAL(0u, GetMaxBlockSizeVote(CScript() << height << to_uchar("BIP100/B2/"), height));

    // whitespace is not allowed
    BOOST_CHECK_EQUAL(0u, GetMaxBlockSizeVote(CScript() << height << to_uchar("/ EB2/"), height));
    BOOST_CHECK_EQUAL(0u, GetMaxBlockSizeVote(CScript() << height << to_uchar("/EB2 /"), height));
    BOOST_CHECK_EQUAL(0u, GetMaxBlockSizeVote(CScript() << height << to_uchar("/EB 2/"), height));
    BOOST_CHECK_EQUAL(0u, GetMaxBlockSizeVote(CScript() << height << to_uchar("/BIP100/B2 /"), height));
    BOOST_CHECK_EQUAL(0u, GetMaxBlockSizeVote(CScript() << height << to_uchar("/BIP100/ B2/"), height));
    BOOST_CHECK_EQUAL(0u, GetMaxBlockSizeVote(CScript() << height << to_uchar("/BIP100/B 2/"), height));
    BOOST_CHECK_EQUAL(0u, GetMaxBlockSizeVote(CScript() << height << to_uchar("/BIP100 /B2/"), height));
    BOOST_CHECK_EQUAL(0u, GetMaxBlockSizeVote(CScript() << height << to_uchar("/ BIP100/B2/"), height));

    // decimals not supported
    BOOST_CHECK_EQUAL(0u, GetMaxBlockSizeVote(CScript() << height << to_uchar("/EB2.2/"), height));
    BOOST_CHECK_EQUAL(0u, GetMaxBlockSizeVote(CScript() << height << to_uchar("/BIP100/B2.2/"), height));

    // missing mb value
    BOOST_CHECK_EQUAL(0u, GetMaxBlockSizeVote(CScript() << to_uchar("/BIP100/B/"), height));
    BOOST_CHECK_EQUAL(0u, GetMaxBlockSizeVote(CScript() << to_uchar("/EB/"), height));

    // missing BIP100 prefix
    BOOST_CHECK_EQUAL(0u, GetMaxBlockSizeVote(CScript() << to_uchar("/B2/"), height));
    BOOST_CHECK_EQUAL(0u, GetMaxBlockSizeVote(CScript() << to_uchar("/BIP100/B/B8/"), height));

    //Explicit zeros and garbage
    BOOST_CHECK_EQUAL(0u, GetMaxBlockSizeVote(CScript() << to_uchar("/BIP100/B0/BIP100/B2"), height));
    BOOST_CHECK_EQUAL(0u, GetMaxBlockSizeVote(CScript() << to_uchar("/EB0/EB2/"), height));
    BOOST_CHECK_EQUAL(0u, GetMaxBlockSizeVote(CScript() << to_uchar("/BIP100/Bgarbage/B2/"), height));
    BOOST_CHECK_EQUAL(2000000u, GetMaxBlockSizeVote(CScript() << to_uchar("/EBgarbage/EB2/"), height));


    // Test that height is not a part of the vote string.
    // Encoded height in this test ends with /.
    // Should not be interpreted as /BIP100/B8/
    CScript coinbase = CScript() << 47;
    BOOST_CHECK_EQUAL('/', coinbase.back());
    std::vector<unsigned char> vote = to_uchar("BIP100/B8/");
    coinbase.insert(coinbase.end(), vote.begin(), vote.end()); // insert instead of << to avoid size being prepended
    BOOST_CHECK_EQUAL(0u, GetMaxBlockSizeVote(coinbase, 47));
}

BOOST_AUTO_TEST_CASE(next_block_raise_cap_bch) {

    const uint64_t newmax = THIRD_HF_INITIAL_MAX_BLOCK_SIZE;

    BOOST_CHECK_EQUAL(newmax, NextBlockRaiseCap(MAX_BLOCK_SIZE));
    BOOST_CHECK_EQUAL(newmax, NextBlockRaiseCap(UAHF_INITIAL_MAX_BLOCK_SIZE));
    BOOST_CHECK_EQUAL(newmax * 105 / 100, NextBlockRaiseCap(newmax));
    BOOST_CHECK_EQUAL(newmax *  10 * 105 / 100, NextBlockRaiseCap(newmax * 10));

    BOOST_CHECK_THROW(NextBlockRaiseCap(MAX_BLOCK_SIZE - 1),
                      std::invalid_argument);
}

BOOST_AUTO_TEST_CASE(next_block_raise_cap_btc) {
    auto arg = new DummyArgGetter;
    auto argraii = SetDummyArgGetter(std::unique_ptr<ArgGetter>(arg));

    arg->Set("-uahftime", 0);

    BOOST_CHECK_EQUAL(MAX_BLOCK_SIZE * 105 / 100, NextBlockRaiseCap(MAX_BLOCK_SIZE));
    BOOST_CHECK_THROW(NextBlockRaiseCap(MAX_BLOCK_SIZE - 1), std::invalid_argument);
}

BOOST_AUTO_TEST_CASE(third_hf_bump) {

    auto params = Params(CBaseChainParams::MAIN).GetConsensus();

    uint64_t currMax = UAHF_INITIAL_MAX_BLOCK_SIZE;

    std::vector<CBlockIndex> blocks(5);
    fillBlockIndex(params, blocks, false, currMax);
    blocks[2].nTime = Opt().ThirdHFTime();
    blocks[3].nTime = blocks[2].nTime + 1;
    blocks[4].nTime = blocks[3].nTime + 1;

    for (size_t i = 1; i < blocks.size(); ++i) {
        blocks[i].nMaxBlockSize = GetNextMaxBlockSize(&blocks[i - 1], params);
    }

    BOOST_CHECK_EQUAL(currMax, GetNextMaxBlockSize(&blocks[0], params));
    BOOST_CHECK_EQUAL(currMax, GetNextMaxBlockSize(&blocks[1], params));
    // block is at HF time, but MTP isn't.
    BOOST_CHECK_EQUAL(currMax, GetNextMaxBlockSize(&blocks[2], params));
    // MTP passes fork point
    BOOST_CHECK_EQUAL(THIRD_HF_INITIAL_MAX_BLOCK_SIZE, GetNextMaxBlockSize(&blocks[3], params));
    BOOST_CHECK_EQUAL(THIRD_HF_INITIAL_MAX_BLOCK_SIZE, GetNextMaxBlockSize(&blocks[4], params));
}

BOOST_AUTO_TEST_SUITE_END();
