// Copyright (c) 2016- The Bitcoin XT developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#include <boost/test/unit_test.hpp>
#include "test/thinblockutil.h"
#include "thinblockmanager.h"
#include "uint256.h"
#include "xthin.h"
#include "chainparams.h"
#include <memory>

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
    worker->setToWork(block);
    BOOST_CHECK_EQUAL(1, mg->numWorkers(block));

    worker->setAvailable();
    BOOST_CHECK_EQUAL(0, mg->numWorkers(block));

    worker->setToWork(block);
    worker.reset();
    BOOST_CHECK_EQUAL(0, mg->numWorkers(block));
};

BOOST_AUTO_TEST_SUITE_END();
