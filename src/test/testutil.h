#ifndef BITCOIN_TESTUTIL_H
#define BITCOIN_TESTUTIL_H

#include "main.h"
#include <boost/filesystem/path.hpp>
#include <functional>
#include <stdexcept>
#include <string>
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

boost::filesystem::path GetTempPath();

// Used with BOOST_CHECK_EXCEPTION for checking contents of exception string
std::function<bool(const std::invalid_argument&)> errorContains(
        const std::string& str);

#endif // BITCOIN_TEST_TESTUTIL_H
