// Copyright (c) 2016-2017 The Bitcoin XT developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#include <boost/test/unit_test.hpp>
#include "test/dummyconnman.h"
#include "test/thinblockutil.h"
#include "thinblockmanager.h"
#include "uint256.h"
#include "xthin.h"
#include "compactthin.h"
#include "chainparams.h"
#include <memory>
#include <iostream>

// Workaround for segfaulting
struct Workaround {
    Workaround() {
        SelectParams(CBaseChainParams::MAIN);
    }
};

BOOST_FIXTURE_TEST_SUITE(thinblockmanager_tests, Workaround);

BOOST_AUTO_TEST_CASE(add_and_del_worker) {
    std::unique_ptr<ThinBlockManager> mg = GetDummyThinBlockMg();

    std::unique_ptr<ThinBlockWorker> worker(new XThinWorker(*mg, 42));

    // Assigning a worker to a block adds it to the manager.
    uint256 block = uint256S("0xFF");
    worker->addWork(block);
    BOOST_CHECK_EQUAL(1, mg->numWorkers(block));

    worker->stopWork(block);
    BOOST_CHECK_EQUAL(0, mg->numWorkers(block));

    worker->addWork(block);
    worker.reset();
    BOOST_CHECK_EQUAL(0, mg->numWorkers(block));
};

struct DummyAnn : public BlockAnnHandle {
    DummyAnn(NodeId id, std::vector<NodeId>& announcers)
        : id(id), announcers(announcers)
    {
        announcers.push_back(id);
    }
    ~DummyAnn() {
        auto i = std::find(begin(announcers), end(announcers), id);
        if (i != end(announcers))
            announcers.erase(i);
    }
    virtual NodeId nodeID() const { return id; }

    NodeId id;
    std::vector<NodeId>& announcers;
};


struct BlockAnnWorker : public ThinBlockWorker {
    BlockAnnWorker(ThinBlockManager& m, NodeId i, std::vector<NodeId>& a)
        : ThinBlockWorker(m, i), announcers(a)
    {
    }
    std::unique_ptr<BlockAnnHandle> requestBlockAnnouncements(CConnman&, CNode&) override {
        return std::unique_ptr<DummyAnn>(
                new DummyAnn(nodeID(), announcers));
    }
    void requestBlock(const uint256& block,
                      std::vector<CInv>& getDataReq,
                      CConnman&, CNode& node) override { }

    std::vector<NodeId>& announcers;
};

BOOST_AUTO_TEST_CASE(request_block_announcements) {


    std::unique_ptr<ThinBlockManager> mg = GetDummyThinBlockMg();
    std::vector<NodeId> announcers;
    DummyConnman c;
    DummyNode n;

    BlockAnnWorker w1(*mg, 11, announcers);
    BlockAnnWorker w2(*mg, 12, announcers);
    BlockAnnWorker w3(*mg, 13, announcers);
    mg->requestBlockAnnouncements(w1, c, n);
    mg->requestBlockAnnouncements(w2, c, n);
    mg->requestBlockAnnouncements(w3, c, n);

    // We want 3 block announcers, so all should have been kept.
    std::vector<NodeId> expected = { 11, 12, 13 };
    BOOST_CHECK_EQUAL_COLLECTIONS(
        begin(announcers), end(announcers),
        begin(expected), end(expected));

    // Move 11 to the front,
    mg->requestBlockAnnouncements(w1, c, n);

    // ...which means 14 should bump 12 out
    BlockAnnWorker w4(*mg, 14, announcers);
    mg->requestBlockAnnouncements(w4, c, n);
    expected = { 11, 13, 14 };
    BOOST_CHECK_EQUAL_COLLECTIONS(
        begin(announcers), end(announcers),
        begin(expected), end(expected));

    // Requesting from a node that does not support
    // block announcements should have no effect.
    struct DummyWorker : public ThinBlockWorker {
        DummyWorker(ThinBlockManager& m, NodeId i) : ThinBlockWorker(m, i) { }
        void requestBlock(const uint256& block,
                          std::vector<CInv>& getDataReq,
                          CConnman&, CNode& node) override { }
    };
    DummyWorker w5(*mg, 15);
    mg->requestBlockAnnouncements(w5, c, n);
    BOOST_CHECK_EQUAL_COLLECTIONS(
        begin(announcers), end(announcers),
        begin(expected), end(expected));
}

BOOST_AUTO_TEST_CASE(add_and_delete_workers) {
    std::unique_ptr<ThinBlockManager> mg = GetDummyThinBlockMg();

    CompactWorker w1(*mg, 42);
    XThinWorker w2(*mg, 43);

    uint256 blockA = uint256S("0xf00");
    uint256 blockB = uint256S("0xbaa");

    mg->addWorker(blockA, w1);
    mg->addWorker(blockB, w1);
    mg->addWorker(blockA, w2);

    BOOST_CHECK_EQUAL(2, mg->numWorkers(blockA));
    BOOST_CHECK_EQUAL(1, mg->numWorkers(blockB));

    mg->delWorker(blockA, w1);
    BOOST_CHECK_EQUAL(1, mg->numWorkers(blockA));

    mg->delWorker(blockA, w2);
    mg->delWorker(blockB, w1);
    BOOST_CHECK_EQUAL(0, mg->numWorkers(blockA));
    BOOST_CHECK_EQUAL(0, mg->numWorkers(blockB));
}

BOOST_AUTO_TEST_SUITE_END();
