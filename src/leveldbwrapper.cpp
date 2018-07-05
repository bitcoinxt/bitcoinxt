#include "dbwrapper.h"
#include "leveldbwrapper.h"
#include "util.h"

#include <boost/filesystem.hpp>

#include <leveldb/cache.h>
#include <leveldb/db.h>
#include <leveldb/env.h>
#include <leveldb/filter_policy.h>
#include <leveldb/write_batch.h>
#include <memenv.h>
#include <algorithm>

namespace {

class CBitcoinLevelDBLogger : public leveldb::Logger {
public:
    // This code is adapted from posix_logger.h, which is why it is using vsprintf.
    // Please do not do this in normal code
    void Logv(const char * format, va_list ap) override {
            if (!LogAcceptCategory(Log::LEVELDB)) {
                return;
            }
            char buffer[500];
            for (int iter = 0; iter < 2; iter++) {
                char* base;
                int bufsize;
                if (iter == 0) {
                    bufsize = sizeof(buffer);
                    base = buffer;
                }
                else {
                    bufsize = 30000;
                    base = new char[bufsize];
                }
                char* p = base;
                char* limit = base + bufsize;

                // Print the message
                if (p < limit) {
                    va_list backup_ap;
                    va_copy(backup_ap, ap);
                    // Do not use vsnprintf elsewhere in bitcoin source code, see above.
                    p += vsnprintf(p, limit - p, format, backup_ap);
                    va_end(backup_ap);
                }

                // Truncate to available space if necessary
                if (p >= limit) {
                    if (iter == 0) {
                        continue;       // Try again with larger buffer
                    }
                    else {
                        p = limit - 1;
                    }
                }

                // Add newline if necessary
                if (p == base || p[-1] != '\n') {
                    *p++ = '\n';
                }

                assert(p <= limit);
                base[std::min(bufsize - 1, (int)(p - base))] = '\0';
                LogPrintf("leveldb: %s", base);
                if (base != buffer) {
                    delete[] base;
                }
                break;
            }
    }
};

leveldb::Options GetOptions(size_t nCacheSize)
{
    leveldb::Options options;
    options.block_cache = leveldb::NewLRUCache(nCacheSize / 2);
    options.write_buffer_size = nCacheSize / 4; // up to two write buffers may be held in memory simultaneously
    options.filter_policy = leveldb::NewBloomFilterPolicy(10);
    options.compression = leveldb::kNoCompression;
    options.max_open_files = 64;
    options.info_log = new CBitcoinLevelDBLogger();
    if (leveldb::kMajorVersion > 1 || (leveldb::kMajorVersion == 1 && leveldb::kMinorVersion >= 16)) {
        // LevelDB versions before 1.16 consider short writes to be corruption. Only trigger error
        // on corruption in later versions.
        options.paranoid_checks = true;
    }
    return options;
}

void HandleError(const leveldb::Status& status)
{
    if (status.ok())
        return;
    LogPrintf("%s\n", status.ToString());
    if (status.IsCorruption())
        throw dbwrapper_error("Database corrupted");
    if (status.IsIOError())
        throw dbwrapper_error("Database I/O error");
    if (status.IsNotFound())
        throw dbwrapper_error("Database entry missing");
    throw dbwrapper_error("Unknown database error");
}

/** Batch of changes queued to be written to a CDBWrapper */
class LevelDBBatch : public CDBBatch
{
    friend class LevelDBWrapper;

private:
    leveldb::WriteBatch batch;
    size_t size_estimate;

public:
    LevelDBBatch() : size_estimate(0) { };

    void Clear()
    {
        batch.Clear();
        size_estimate = 0;
    }

    void Write(const CDataStream& key, const CDataStream& value) override
    {
        leveldb::Slice slKey(key.data(), key.size());
        leveldb::Slice slValue(value.data(), value.size());

        batch.Put(slKey, slValue);
        // LevelDB serializes writes as:
        // - byte: header
        // - varint: key length (1 byte up to 127B, 2 bytes up to 16383B, ...)
        // - byte[]: key
        // - varint: value length
        // - byte[]: value
        // The formula below assumes the key and value are both less than 16k.
        size_estimate += 3 + (slKey.size() > 127) + slKey.size() + (slValue.size() > 127) + slValue.size();
    }

    void Erase(const CDataStream& key) override {
        leveldb::Slice slKey(key.data(), key.size());

        batch.Delete(slKey);
        // LevelDB serializes erases as:
        // - byte: header
        // - varint: key length
        // - byte[]: key
        // The formula below assumes the key is less than 16kB.
        size_estimate += 2 + (slKey.size() > 127) + slKey.size();
    }

    size_t SizeEstimate() const { return size_estimate; }
};

class LevelDBIterator : public CDBIterator
{
private:
    leveldb::Iterator *piter;

protected:
    CDataStream GetKey() override {
        leveldb::Slice slKey = piter->key();
        return CDataStream(slKey.data(), slKey.data() + slKey.size(),
                           SER_DISK, CLIENT_VERSION);
    }

    CDataStream GetValue() override {
        leveldb::Slice slValue = piter->value();
        CDataStream ssValue(slValue.data(), slValue.data() + slValue.size(),
                            SER_DISK, CLIENT_VERSION);
        return ssValue;
    }

public:
    LevelDBIterator(leveldb::Iterator *piterIn) : piter(piterIn) {}
    ~LevelDBIterator() {
        delete piter;
    }

    bool Valid() override {
        return piter->Valid();
    }

    void SeekToFirst() override {
        piter->SeekToFirst();
    }

    void Seek(const CDataStream& ssKey) override {
        leveldb::Slice slKey(ssKey.data(), ssKey.size());
        piter->Seek(slKey);
    }

    void Next() override {
        piter->Next();
    }

    unsigned int GetKeySize() override {
        return piter->key().size();
    }


    unsigned int GetValueSize() override {
        return piter->value().size();
    }
};

class LevelDBWrapper : public CDBWrapper {
private:
    //! custom environment this database is using (may be NULL in case of default environment)
    leveldb::Env* penv;

    //! database options used
    leveldb::Options options;

    //! options used when reading from the database
    leveldb::ReadOptions readoptions;

    //! options used when iterating over values of the database
    leveldb::ReadOptions iteroptions;

    //! options used when writing to the database
    leveldb::WriteOptions writeoptions;

    //! options used when sync writing to the database
    leveldb::WriteOptions syncoptions;

    //! the database itself
    leveldb::DB* pdb;

    //! the key under which a obfuscation key may be stored by a future version of DBWrapper
    static const std::string OBFUSCATE_KEY_KEY;

protected:
    CDataStream Read(const CDataStream& ssKey) const override {
        leveldb::Slice slKey(ssKey.data(), ssKey.size());

        std::string strValue;
        leveldb::Status status = pdb->Get(readoptions, slKey, &strValue);
        if (!status.ok()) {
            if (status.IsNotFound())
                throw dbwrapper_notfound{};
            LogPrintf("LevelDB read failure: %s\n", status.ToString());
            HandleError(status);
        }
        return CDataStream(strValue.data(), strValue.data() + strValue.size(),
                           SER_DISK, CLIENT_VERSION);
    }

    bool Exists(const CDataStream& key) const override {
        leveldb::Slice slKey(key.data(), key.size());

        std::string strValue;
        leveldb::Status status = pdb->Get(readoptions, slKey, &strValue);
        if (!status.ok()) {
            if (status.IsNotFound())
                return false;
            LogPrintf("LevelDB read failure: %s\n", status.ToString());
            HandleError(status);
        }
        return true;
    }
public:
    LevelDBWrapper(const boost::filesystem::path& path, size_t nCacheSize,
                   bool &isObfuscated, bool fMemory, bool fWipe)
    {
        penv = NULL;
        readoptions.verify_checksums = true;
        iteroptions.verify_checksums = true;
        iteroptions.fill_cache = false;
        syncoptions.sync = true;
        options = GetOptions(nCacheSize);
        options.create_if_missing = true;
        if (fMemory) {
            penv = leveldb::NewMemEnv(leveldb::Env::Default());
            options.env = penv;
        } else {
            if (fWipe) {
                LogPrintf("Wiping LevelDB in %s\n", path.string());
                leveldb::Status result = leveldb::DestroyDB(path.string(), options);
                HandleError(result);
            }
            TryCreateDirectory(path);
            LogPrintf("Opening LevelDB in %s\n", path.string());
        }
        leveldb::Status status = leveldb::DB::Open(options, path.string(), &pdb);
        HandleError(status);
        LogPrintf("Opened LevelDB successfully\n");

        std::vector<unsigned char> obfuscate_key;
        isObfuscated = CDBWrapper::Read(OBFUSCATE_KEY_KEY, obfuscate_key);
    }

    ~LevelDBWrapper() {
        delete pdb;
        pdb = NULL;
        delete options.filter_policy;
        options.filter_policy = NULL;
        delete options.info_log;
        options.info_log = NULL;
        delete options.block_cache;
        options.block_cache = NULL;
        delete penv;
        options.env = NULL;
    }

    bool WriteBatch(CDBBatch& batch, bool fSync = false) override {
        // FIXME: Use of dynamic_cast is a sign of bad design.
        LevelDBBatch* lbatch = dynamic_cast<LevelDBBatch*>(&batch);
        if (lbatch == nullptr) {
            throw std::invalid_argument("Invalid batch type for WriteBatch");
        }
        return WriteBatch(*lbatch, fSync);
    }

    bool WriteBatch(LevelDBBatch& batch, bool fSync = false) {
        leveldb::Status status = pdb->Write(fSync ? syncoptions : writeoptions, &batch.batch);
        HandleError(status);
        return true;
    }

    // not available for LevelDB
    bool Flush() override
    {
        return true;
    }

    bool Sync() override
    {
        LevelDBBatch batch;
        return WriteBatch(batch, true);
    }

    CDBIterator* NewIterator() override
    {
        return new LevelDBIterator(pdb->NewIterator(iteroptions));
    }

    std::unique_ptr<CDBBatch> NewBatch() override {
        return std::unique_ptr<CDBBatch>(new LevelDBBatch);
    }

    size_t EstimateSize() const override
    {
        const char DB_COIN = 'C';
        const char DB_COIN_END = DB_COIN + 1;
        leveldb::Slice slKey1(&DB_COIN);
        leveldb::Slice slKey2(&DB_COIN_END);
        uint64_t size = 0;
        leveldb::Range range(slKey1, slKey2);
        pdb->GetApproximateSizes(&range, 1, &size);
        return size;
    }
};
const std::string LevelDBWrapper::OBFUSCATE_KEY_KEY("\000obfuscate_key", 14);

} // ns anon

std::unique_ptr<CDBWrapper> CreateLevelDB(
        const boost::filesystem::path& path, size_t nCacheSize,
        bool &isObfuscated, bool fMemory, bool fWipe)
{
    return std::unique_ptr<CDBWrapper>(new LevelDBWrapper(path, nCacheSize, isObfuscated, fMemory, fWipe));
}
