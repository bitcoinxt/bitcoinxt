// Copyright (c) 2016- The Bitcoin XT developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#include <boost/test/unit_test.hpp>
#include "test/thinblockutil.h"
#include "blockencodings.h"
#include "blocksender.h"
#include "net.h"
#include "uint256.h"
#include "protocol.h"
#include "chain.h"
#include "xthin.h"
#include <string>

using namespace std;

BOOST_AUTO_TEST_SUITE(blocksender_tests);

BOOST_AUTO_TEST_CASE(inv_type_check) {
    BlockSender b;
    BOOST_CHECK(b.isBlockType(MSG_BLOCK));
    BOOST_CHECK(b.isBlockType(MSG_FILTERED_BLOCK));
    BOOST_CHECK(b.isBlockType(MSG_THINBLOCK));
    BOOST_CHECK(b.isBlockType(MSG_XTHINBLOCK));
    BOOST_CHECK(b.isBlockType(MSG_CMPCT_BLOCK));
    BOOST_CHECK(!b.isBlockType(MSG_TX));
}

BOOST_AUTO_TEST_CASE(trigger_next_request) {

    uint256 block1 = uint256S("0xf00d");
    uint256 block2 = uint256S("0xbeef");

    struct BS : public BlockSender {
        // make method public
        void triggerNextRequest(const CChain& active, const CInv& inv, CNode& node) {
            BlockSender::triggerNextRequest(active, inv, node);
        }
    };

    DummyNode node1;
    node1.hashContinue = block1;
    DummyNode node2;
    node2.hashContinue = block1;

    CChain active;
    uint256 activeTip = uint256S("0xfade");
    CBlockIndex tip;
    tip.phashBlock = &activeTip;
    active.SetTip(&tip);

    // should trigger if (and only if) block hash is same as hashContinue
    BS b;
    b.triggerNextRequest(active, CInv(MSG_BLOCK, block1), node1);
    BOOST_CHECK_EQUAL("inv", node1.messages.at(0));

    // different hash, should not trigger
    b.triggerNextRequest(active, CInv(MSG_BLOCK, block2), node2);
    BOOST_CHECK_EQUAL(0, node2.ssSend.size());
}

BOOST_AUTO_TEST_CASE(can_send) {
    BlockSender b;

    CChain activeChain;

    std::vector<CBlockIndex> blocks(2);
    blocks[0].nHeight = 0;
    blocks[1].nHeight = 1;
    blocks[0].pprev = NULL;
    blocks[1].pprev = &blocks[0];

    CBlockIndex notInActive;
    notInActive.nHeight = 1;

    activeChain.SetTip(&blocks.back());

    // don't have data
    blocks[0].nStatus = 0;
    BOOST_CHECK(!b.canSend(activeChain, blocks[0], NULL));

    // have data and in active chain
    blocks[0].nStatus = BLOCK_HAVE_DATA;
    BOOST_CHECK(b.canSend(activeChain, blocks[0], NULL));

    // have data, not in active chain
    notInActive.nStatus = BLOCK_HAVE_DATA;
    BOOST_CHECK(!b.canSend(activeChain, notInActive, NULL));

    // FIXME: Add test for fingerprint prevention logic
}

struct BlockSenderDummy : public BlockSender {
    BlockSenderDummy() : BlockSender(), readBlock(TestBlock1())
    {
    }

    virtual bool readBlockFromDisk(CBlock& block, const CBlockIndex* pindex) {
        block = readBlock;
        return true;
    }
    CBlock readBlock;
};

BOOST_AUTO_TEST_CASE(send_msg_block) {
    CBlockIndex index;
    BlockSenderDummy bs;
    DummyNode node;

    bs.sendBlock(node, index, MSG_BLOCK, index.nHeight);
    BOOST_CHECK_EQUAL("block", node.messages.at(0));
}

// We don't support this message, so we fallback to sending
// full block instead.
BOOST_AUTO_TEST_CASE(send_msg_thinblock) {
    CBlockIndex index;
    BlockSenderDummy bs;
    DummyNode node;
    NodeStatePtr(node.id)->supportsCompactBlocks = false;

    bs.sendBlock(node, index, MSG_THINBLOCK, index.nHeight);
    BOOST_CHECK_EQUAL("block", node.messages.at(0));
}

BOOST_AUTO_TEST_CASE(send_msg_xthinblock) {
    CBlockIndex index;
    BlockSenderDummy bs;

    // Case 1 - if a thin block is larger than a full block,
    // send full block. This happens for example when there are no filtered transactions
    DummyNode node;
    node.xthinFilter->clear();
    bs.readBlock = TestBlock2(); // "big" block. No transactions filtered!
    bs.sendBlock(node, index, MSG_XTHINBLOCK, index.nHeight);
    BOOST_CHECK_EQUAL("block", node.messages.at(0));

    // Case 2 - send block with filtered transactions.
    DummyNode node2;
    bs.readBlock = TestBlock2();
    CBloomFilter* filter = new CBloomFilter(3, 0.01, 2147483649UL, BLOOM_UPDATE_ALL);
    filter->insert(bs.readBlock.vtx[0].GetHash());
    filter->insert(bs.readBlock.vtx[1].GetHash());
    filter->insert(bs.readBlock.vtx[2].GetHash());
    node2.xthinFilter.reset(filter);
    bs.sendBlock(node2, index, MSG_XTHINBLOCK, index.nHeight);
    BOOST_CHECK_EQUAL("xthinblock", node2.messages.at(0));
}

BOOST_AUTO_TEST_CASE(send_msg_filteredblock) {
    CBlockIndex index;
    BlockSenderDummy bs;
    DummyNode node;
    bs.sendBlock(node, index, MSG_FILTERED_BLOCK, index.nHeight);
    BOOST_CHECK_EQUAL("merkleblock", node.messages.at(0));
}

BOOST_AUTO_TEST_CASE(send_msg_xblocktx) {
    CBlockIndex index;
    BlockSenderDummy bs;
    DummyNode node;
    CBlock block = TestBlock1();

    std::set<uint64_t> requesting;
    requesting.insert(block.vtx[1].GetHash().GetCheapHash());
    requesting.insert(block.vtx[2].GetHash().GetCheapHash());
    XThinReRequest req;
    req.txRequesting = requesting;
    bs.sendReReqReponse(node, index, req);
    BOOST_CHECK_EQUAL("xblocktx", node.messages.at(0));
}

BOOST_AUTO_TEST_CASE(send_msg_cmpct_block) {
    CBlockIndex index;
    BlockSenderDummy bs;

    DummyNode node;
    NodeStatePtr(node.id)->supportsCompactBlocks = true;
    bs.readBlock = TestBlock2();
    bs.sendBlock(node, index, MSG_CMPCT_BLOCK, index.nHeight);
    BOOST_CHECK_EQUAL("cmpctblock", node.messages.at(0));
};

BOOST_AUTO_TEST_CASE(send_old_cmpct_block) {
    // If someone requests an old compact block,
    // send them an full block instead.
    CBlockIndex index;
    BlockSenderDummy bs;

    DummyNode node;
    NodeStatePtr(node.id)->supportsCompactBlocks = true;
    bs.readBlock = TestBlock2();
    index.nHeight = 89;
    bs.sendBlock(node, index, MSG_CMPCT_BLOCK, index.nHeight + 11);
    BOOST_CHECK_EQUAL("block", node.messages.at(0));

}

BOOST_AUTO_TEST_CASE(send_msg_blocktxn) {
    CBlockIndex index;
    BlockSenderDummy bs;
    DummyNode node;
    CBlock block = TestBlock1();

    CompactReRequest req;
    req.indexes.push_back(1);
    req.indexes.push_back(4);
    req.blockhash = block.GetHash();
    bs.sendReReqReponse(node, index, req);
    BOOST_CHECK_EQUAL("blocktxn", node.messages.at(0));
}


BOOST_AUTO_TEST_SUITE_END();
