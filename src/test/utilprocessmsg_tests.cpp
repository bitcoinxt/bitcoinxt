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
        usethin(NOT_SET)
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
        if (arg == "-use-thin-blocks")
            return usethin == NOT_SET ? def : usethin;

        return def;
    }

    int usethin;
};

BOOST_AUTO_TEST_SUITE(utilprocessmsg_tests);

BOOST_AUTO_TEST_CASE(keep_outgoing_peer_thin) {

    auto arg = new UtilDummyArgGetter;
    auto argraii = SetDummyArgGetter(std::unique_ptr<ArgGetter>(arg));

    // Node that does not support thin blocks.
    DummyNode node;
    node.nServices = 0;
    node.nVersion = SENDHEADERS_VERSION; //< version before compact blocks

    // Thin blocks disabled. Still a keeper.
    arg->usethin = 0;
    BOOST_CHECK(KeepOutgoingPeer(node));

    // Thin block enabled. Node does not support it.
    arg->usethin = 1;
    BOOST_CHECK(!KeepOutgoingPeer(node));

    // Node supports bloom thin blocks, keep.
    node.nServices = NODE_BLOOM | NODE_GETUTXO;
    BOOST_CHECK(KeepOutgoingPeer(node));

    // Node supports xthin, but not bloom, keep.
    node.nServices = NODE_THIN;
    BOOST_CHECK(KeepOutgoingPeer(node));

    // We want optimal thin blocks only, disconnect.
    arg->usethin = 3;
    node.nServices = NODE_BLOOM | NODE_GETUTXO;
    BOOST_CHECK(!KeepOutgoingPeer(node));

    // Node supports xthin, keep.
    node.nServices = NODE_THIN;
    BOOST_CHECK(KeepOutgoingPeer(node));

    // Node supports compact blocks, keep.
    node.nServices = 0;
    node.nServices = SHORT_IDS_BLOCKS_VERSION;
    BOOST_CHECK(KeepOutgoingPeer(node));
}

BOOST_AUTO_TEST_CASE(keep_outgoing_peer_uasf) {
    DummyNode node;
    node.nServices = 0;
    auto arg = new UtilDummyArgGetter;
    auto argraii = SetDummyArgGetter(std::unique_ptr<ArgGetter>(arg));
    arg->usethin = 0;

    node.strSubVer = "Nice node";
    BOOST_CHECK(KeepOutgoingPeer(node));

    node.strSubVer = "Bad UASF node";
    BOOST_CHECK(!KeepOutgoingPeer(node));
};

BOOST_AUTO_TEST_SUITE_END()
