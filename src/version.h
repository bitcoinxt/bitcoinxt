// Copyright (c) 2012-2014 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_VERSION_H
#define BITCOIN_VERSION_H

// disconnect from peers older than this protocol version.
static const int MIN_PEER_PROTO_VERSION = 70010;

// p2p network protocol version that this node implements.
static const int PROTOCOL_VERSION = MIN_PEER_PROTO_VERSION + 0;

// initial protocol version used between peers. the version used between peers may be increased by version/verack negotiation.
static const int INIT_PROTO_VERSION = 209;

#endif // BITCOIN_VERSION_H
