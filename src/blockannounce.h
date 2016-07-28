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
        
class BlockAnnounceProcessor {

    public:
        BlockAnnounceProcessor(uint256 block,
                CNode& from, ThinBlockManager& thinmg, InFlightIndex& inFlightIndex) : 
            block(block), from(from), thinmg(thinmg), blocksInFlight(inFlightIndex)
        {
        }

        bool onBlockAnnounced(std::vector<CInv>& toFetch);
        
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
        virtual bool wantBlock() const;
        virtual bool isInitialBlockDownload() const;
       

    private:
        uint256 block;
        CNode& from;
        ThinBlockManager& thinmg;
        InFlightIndex& blocksInFlight;

        bool fetchAsThin() const;
};

#endif
