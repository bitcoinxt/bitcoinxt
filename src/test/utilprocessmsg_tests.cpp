// Copyright (c) 2016 The Bitcoin XT developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <boost/test/unit_test.hpp>

#include "utilprocessmsg.h"
#include "options.h"
#include "test/thinblockutil.h"

#include <limits.h>

const int NOT_SET = std::numeric_limits<int>::min();

struct UtilDummyArgGetter : public ArgGetter {

    UtilDummyArgGetter() : ArgGetter(),
        useselection(NOT_SET), usethin(NOT_SET)
    {
    }

    int64_t GetArg(const std::string& arg, int64_t def) override {

        if (arg == "-use-thin-blocks")
            return usethin == NOT_SET ? def : usethin;

        assert(false);
    }

    std::vector<std::string> GetMultiArgs(const std::string& arg) override {
        assert(false);
    }

    bool GetBool(const std::string& arg, bool def) override {
        if (arg == "-use-peer-selection")
            return useselection == NOT_SET ? def : useselection;
        if (arg == "-use-thin-blocks")
            return usethin == NOT_SET ? def : usethin;

        return def;
    }

    int useselection;
    int usethin;
};

BOOST_AUTO_TEST_SUITE(utilprocessmsg_tests);

BOOST_AUTO_TEST_CASE(keep_outgoing_peer) {

    auto arg = new UtilDummyArgGetter;
    auto argraii = SetDummyArgGetter(std::unique_ptr<ArgGetter>(arg));

    // Node that does not support thin blocks.
    DummyNode node;
    node.nServices = 0;

    // Peer selection disabled, it's a keeper.
    arg->useselection = 0;
    arg->usethin = 1;
    BOOST_CHECK(KeepOutgoingPeer(node));

    // Peer selection enabled, but thin blocks disabled. Still a keeper.
    arg->useselection = 1;
    arg->usethin = 0;
    BOOST_CHECK(KeepOutgoingPeer(node));

    // Both enabled, disconnect.
    arg->usethin = 1;
    BOOST_CHECK(!KeepOutgoingPeer(node));

    // Node supports bloom thin blocks, keep.
    node.nServices = NODE_BLOOM | NODE_GETUTXO;
    BOOST_CHECK(KeepOutgoingPeer(node));

    // We want xthin ony, disconnect.
    arg->usethin = 3;
    BOOST_CHECK(!KeepOutgoingPeer(node));

    // Node supports xthin, keep.
    node.nServices = NODE_THIN;
    BOOST_CHECK(KeepOutgoingPeer(node));
}

BOOST_AUTO_TEST_SUITE_END()
