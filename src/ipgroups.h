// Copyright (c) 2009-2015 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_CIPGROUPS_H
#define BITCOIN_CIPGROUPS_H

#include "netbase.h"
#include "sync.h"
#include <boost/noncopyable.hpp>
#include <boost/weak_ptr.hpp>

#define IP_PRIO_SRC_FLAG_NAME "-ip-priority-source"

class CScheduler;

static const int DEFAULT_IPGROUP_PRI = 0; //< same as an IP with 1 connection
static const int IPGROUP_CONN_MODIFIER = 1; //< how much priority is modified per connection in group

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

    // If priority should be modified per connection in group
    bool decrPriority;

    // Group should be erased when connCount goes to 0
    bool selfErases;
    // Connections currently in group
    int connCount;

    CIPGroupData() : priority(DEFAULT_IPGROUP_PRI), decrPriority(false),
        selfErases(false), connCount(0)
    {
    }

    CIPGroupData(const std::string& name, int pri, bool decr = false, bool erases = false) :
        name(name), priority(pri), decrPriority(decr), selfErases(erases), connCount(0)
    {
    }
};

struct CIPGroup {
    CIPGroupData header;
    std::vector<CSubNet> subnets;
};

// Assigned to each connected node to react to a new connection
// in ipgroup and to react to a node disconnecting.
class IPGroupSlot : boost::noncopyable {
    public:
        IPGroupSlot(const std::string& groupName);
        ~IPGroupSlot();

        CIPGroupData Group();

    private:
        std::string groupName;
        boost::weak_ptr<CCriticalSection> groupCS;
};

CIPGroupData FindGroupForIP(CNetAddr ip);

void InitIPGroupsFromCommandLine();
void InitIPGroups(CScheduler *scheduler);

// Creates a group slot for given IP.
// If IP does not belong to a group then a new group will be created.
std::unique_ptr<IPGroupSlot> AssignIPGroupSlot(const CNetAddr& ip);


#endif //BITCOIN_CIPGROUPS_H
