// Copyright (c) 2016- The Bitcoin XT developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef BITCOIN_BLOCKANNOUNCE_H
#define BITCOIN_BLOCKANNOUNCE_H

#include "uint256.h"
#include <vector>

class CInv;
class CNode;
class ThinBlockManager;
class InFlightIndex;

/// Reacts to a node announcing a block to us
class BlockAnnounceReceiver {

    public:
        BlockAnnounceReceiver(uint256 block,
                CNode& from, ThinBlockManager& thinmg, InFlightIndex& inFlightIndex) : 
            block(block), from(from), thinmg(thinmg), blocksInFlight(inFlightIndex)
        {
        }

        bool onBlockAnnounced(std::vector<CInv>& toFetch, bool announcedAsHeader);
        
        enum DownloadStrategy {
            DOWNL_THIN_NOW,
            DOWNL_FULL_NOW,
            DOWNL_THIN_LATER,
            DONT_DOWNL,
            INVALID
        };


    protected:
        virtual bool almostSynced();
        virtual void updateBlockAvailability();
        virtual DownloadStrategy pickDownloadStrategy();
        virtual bool blockHeaderIsKnown() const;
        virtual bool isInitialBlockDownload() const;
        virtual bool hasBlockData() const;

    private:
        uint256 block;
        CNode& from;
        ThinBlockManager& thinmg;
        InFlightIndex& blocksInFlight;

        bool fetchAsThin() const;
};

// Maximum nu, of headers to announce when relaying blocks with headers message
static const unsigned int MAX_BLOCKS_TO_ANNOUNCE = 8;

/// Sends blockannouncements to node
class BlockAnnounceSender {

    public:
        BlockAnnounceSender(CNode& to) : to(to) { }
        virtual ~BlockAnnounceSender() { }

        void announce();

    protected:
        virtual bool canAnnounceWithHeaders() const;
        virtual bool announceWithHeaders();
        virtual void announceWithInv();

    private:
        bool peerHasHeader(const class CBlockIndex*) const;

        CNode& to;
};

std::vector<uint256> findHeadersToAnnounce(const CBlockIndex* oldTip, const CBlockIndex* newTip);

#endif
