// Copyright (c) 2009-2015 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_CIPGROUPS_H
#define BITCOIN_CIPGROUPS_H

#include "netbase.h"

class CScheduler;

// A group of logically related IP addresses. Useful for banning or deprioritising
// sources of abusive traffic/DoS attacks.
struct CIPGroupData {
    std::string name;
    // A priority score indicates how important this group of IP addresses is to this node.
    // Importance determines which group wins when the node is out of resources. Any IP
    // that is not in a group gets a default priority of zero. Therefore, groups with a priority
    // of less than zero will be ignored or disconnected in order to make room for ungrouped
    // IPs, and groups with a higher priority will be serviced before ungrouped IPs.
    int priority;

    CIPGroupData() : priority(0) {}
};

struct CIPGroup {
    CIPGroupData header;
    std::vector<CSubNet> subnets;
};

// Returns NULL if the IP does not belong to any group.
CIPGroupData FindGroupForIP(CNetAddr ip);

void InitIPGroups(CScheduler *scheduler);

#endif //BITCOIN_CIPGROUPS_H
