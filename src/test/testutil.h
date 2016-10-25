#ifndef BITCOIN_TESTUTIL_H
#define BITCOIN_TESTUTIL_H

#include "main.h"

struct DummyBlockIndexEntry {
DummyBlockIndexEntry(const uint256& hash) : hash(hash) {
    index.nStatus = BLOCK_HAVE_DATA;
    index.phashBlock = &this->hash;
    mapBlockIndex.insert(std::make_pair(hash, &index));
    }
    ~DummyBlockIndexEntry() {
        mapBlockIndex.erase(hash);
    }
    CBlockIndex index;
    uint256 hash;
};

#endif
