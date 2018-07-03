// Copyright (c) 2012-2014 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_DBWRAPPER_H
#define BITCOIN_DBWRAPPER_H

#include "clientversion.h"
#include "serialize.h"
#include "streams.h"
#include "util.h"
#include "version.h"

#include <boost/filesystem/path.hpp>


static const size_t DBWRAPPER_PREALLOC_KEY_SIZE = 64;
static const size_t DBWRAPPER_PREALLOC_VALUE_SIZE = 1024;

class dbwrapper_error : public std::runtime_error
{
public:
    dbwrapper_error(const std::string& msg) : std::runtime_error(msg) {}
};

// this exception should be caught and handled in CDBWrapper
class dbwrapper_notfound : public std::runtime_error {
public:
    dbwrapper_notfound() : std::runtime_error("db: item not found") { }
};

class ClearStreamRAII {
public:
    ClearStreamRAII(CDataStream& s) : s(&s) { }
    ~ClearStreamRAII() { s->clear(); }
private:
    CDataStream* s;
};

/** Batch of changes queued to be written to a CDBWrapper */
class CDBBatch
{
private:
    CDataStream ssKey;
    CDataStream ssValue;

protected:
    // template helper methods for db specifics
    virtual void Write(const CDataStream& key, const CDataStream& value) = 0;
    virtual void Erase(const CDataStream& key) = 0;

public:
    CDBBatch() : ssKey(SER_DISK, CLIENT_VERSION),
                 ssValue(SER_DISK, CLIENT_VERSION)
    {
    }
    virtual ~CDBBatch() { }

    virtual void Clear() = 0;
    virtual size_t SizeEstimate() const = 0;

    template <typename K, typename V>
    void Write(const K& key, const V& value)
    {
        ClearStreamRAII clk(ssKey);
        ClearStreamRAII clv(ssValue);
        ssKey.reserve(DBWRAPPER_PREALLOC_KEY_SIZE);
        ssKey << key;
        ssValue.reserve(DBWRAPPER_PREALLOC_VALUE_SIZE);
        ssValue << value;
        return Write(ssKey, ssValue);
    }

    template <typename K>
    void Erase(const K& key)
    {
        ClearStreamRAII c(ssKey);
        ssKey.reserve(DBWRAPPER_PREALLOC_KEY_SIZE);
        ssKey << key;
        return Erase(ssKey);
    }
};

class CDBIterator
{
protected:
    // template helper methods for db specifics
    virtual CDataStream GetKey() = 0;
    virtual CDataStream GetValue() = 0;
    virtual void Seek(const CDataStream& ssKey) = 0;


public:
    virtual ~CDBIterator() { }

    virtual bool Valid() = 0;

    virtual void SeekToFirst() = 0;

    template<typename K> void Seek(const K& key) {
        CDataStream ssKey(SER_DISK, CLIENT_VERSION);
        ssKey.reserve(DBWRAPPER_PREALLOC_KEY_SIZE);
        ssKey << key;
        return Seek(ssKey);
    }

    virtual void Next() = 0;

    template<typename K> bool GetKey(K& key) {
        try {
            GetKey() >> key;
        } catch(const std::exception &e) {
	    LogPrint(Log::DB, "%s: %s\n", __func__, e.what());
	    return false;
        }
        return true;
    }

    virtual unsigned int GetKeySize() = 0;

    template<typename V> bool GetValue(V& value) {
        try {
            GetValue() >> value;
        } catch(const std::exception &e) {
	    LogPrint(Log::DB, "%s: %s\n", __func__, e.what());
	    return false;
        }
        return true;
    }

    virtual unsigned int GetValueSize() = 0;
};

class CDBWrapper
{
protected:
    // template helper methods for db specifics
    virtual CDataStream Read(const CDataStream& ssKey) const = 0;
    virtual bool Exists(const CDataStream& ssKey) const = 0;
    virtual size_t EstimateSize(const CDataStream& ssKeyBegin,
                                const CDataStream& ssKeyEnd) const = 0;
public:
    virtual ~CDBWrapper();

    template <typename K, typename V>
    bool Read(const K& key, V& value) const
    {
        CDataStream ssKey(SER_DISK, CLIENT_VERSION);
        ssKey.reserve(DBWRAPPER_PREALLOC_KEY_SIZE);
        ssKey << key;
        try {
            Read(ssKey) >> value;
        }
        catch (const dbwrapper_notfound&) {
            // It's OK to try to read an entry that doesn't exist.
            return false;
        }
        catch (const std::exception&) {
            return false;
        }
        return true;
    }

    template <typename K, typename V>
    bool Write(const K& key, const V& value, bool fSync = false)
    {
        std::unique_ptr<CDBBatch> batch = NewBatch();
        batch->Write(key, value);
        return WriteBatch(*batch, fSync);
    }

    template <typename K>
    bool Exists(const K& key) const
    {
        CDataStream ssKey(SER_DISK, CLIENT_VERSION);
        ssKey.reserve(DBWRAPPER_PREALLOC_KEY_SIZE);
        ssKey << key;
        return Exists(ssKey);
    }

    template <typename K>
    bool Erase(const K& key, bool fSync = false)
    {
        std::unique_ptr<CDBBatch> batch = NewBatch();
        batch->Erase(key);
        return WriteBatch(*batch, fSync);
    }

    virtual bool WriteBatch(CDBBatch& batch, bool fSync = false) = 0;
    virtual bool Flush() = 0;
    virtual bool Sync() = 0;

    virtual CDBIterator* NewIterator() = 0;
    virtual std::unique_ptr<CDBBatch> NewBatch() = 0;

    /**
     * Return true if the database managed by this class contains no entries.
     */
    bool IsEmpty();

    template<typename K>
    size_t EstimateSize(const K& key_begin, const K& key_end) const
    {
        CDataStream ssKey1(SER_DISK, CLIENT_VERSION), ssKey2(SER_DISK, CLIENT_VERSION);
        ssKey1.reserve(DBWRAPPER_PREALLOC_KEY_SIZE);
        ssKey2.reserve(DBWRAPPER_PREALLOC_KEY_SIZE);
        ssKey1 << key_begin;
        ssKey2 << key_end;
        return EstimateSize(ssKey1, ssKey2);
    }
};

#endif // BITCOIN_DBWRAPPER_H
