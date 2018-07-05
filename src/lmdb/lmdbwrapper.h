#ifndef BITCOIN_LMDBWRAPPER_H
#define BITCOIN_LMDBWRAPPER_H

#include "config/bitcoin-config.h" // for USE_LMDB

#include <memory>
#include <cstddef>
#include <stdexcept>

class CDBWrapper;
namespace boost { namespace filesystem { class path; } }

namespace lmdb {

#ifdef USE_LMDB
std::unique_ptr<CDBWrapper> CreateLMDB(
        const boost::filesystem::path& path,
        bool fMemory = false, bool fWipe = false, bool fSafeMode = true);
#else
std::unique_ptr<CDBWrapper> CreateLMDB(const boost::filesystem::path&, bool, bool, bool) {
    throw std::runtime_error(
            "LMDB support not compiled into binary. Cannot use LMDB");
}
#endif // USE_LMDB

} // ns lmdb

#endif
