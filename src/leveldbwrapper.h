#ifndef BITCOIN_LEVELDBWRAPPER_H
#define BITCOIN_LEVELDBWRAPPER_H

#include <memory>
#include <cstddef>

class CDBWrapper;
namespace boost { namespace filesystem { class path; } }

std::unique_ptr<CDBWrapper> CreateLevelDB(
        const boost::filesystem::path& path, size_t nCacheSize,
        bool &isObfuscated, bool fMemory = false, bool fWipe = false);

#endif
