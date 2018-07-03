// Copyright (c) 2012-2015 The Bitcoin Core developers
// Copyright (c) 2017 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "dbwrapper.h"
#include "leveldbwrapper.h"
#include "random.h"
#include "uint256.h"
#include "test/test_bitcoin.h"

#include <boost/assign/std/vector.hpp> // for 'operator+=()'
#include <boost/assert.hpp>
#include <boost/test/unit_test.hpp>

using namespace std;
using namespace boost::assign; // bring 'operator+=()' into scope
using namespace boost::filesystem;

// Test if a string consists entirely of null characters
bool is_null_key(const vector<unsigned char>& key) {
    bool isnull = true;

    for (unsigned int i = 0; i < key.size(); i++)
        isnull &= (key[i] == '\x00');

    return isnull;
}

BOOST_FIXTURE_TEST_SUITE(dbwrapper_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(dbwrapper)
{
    path ph = temp_directory_path() / unique_path();
    bool isObfuscated;
    std::unique_ptr<CDBWrapper> dbw(CreateLevelDB(ph, (1 << 20), isObfuscated, true, false));
    char key = 'k';
    uint256 in = GetRandHash();
    uint256 res;

    BOOST_CHECK(!isObfuscated);
    BOOST_CHECK(dbw->Write(key, in));
    BOOST_CHECK(dbw->Read(key, res));
    BOOST_CHECK_EQUAL(res.ToString(), in.ToString());
}

// Test batch operations
BOOST_AUTO_TEST_CASE(dbwrapper_batch)
{
    path ph = temp_directory_path() / unique_path();
    bool isObfuscated;
    std::unique_ptr<CDBWrapper> dbw(CreateLevelDB(ph, (1 << 20), isObfuscated, true, false));

    char key = 'i';
    uint256 in = GetRandHash();
    char key2 = 'j';
    uint256 in2 = GetRandHash();
    char key3 = 'k';
    uint256 in3 = GetRandHash();

    uint256 res;
    std::unique_ptr<CDBBatch> batch(dbw->NewBatch());

    batch->Write(key, in);
    batch->Write(key2, in2);
    batch->Write(key3, in3);

    // Remove key3 before it's even been written
    batch->Erase(key3);

    dbw->WriteBatch(*batch);

    BOOST_CHECK(dbw->Read(key, res));
    BOOST_CHECK_EQUAL(res.ToString(), in.ToString());
    BOOST_CHECK(dbw->Read(key2, res));
    BOOST_CHECK_EQUAL(res.ToString(), in2.ToString());

    // key3 never should've been written
    BOOST_CHECK(dbw->Read(key3, res) == false);
}

BOOST_AUTO_TEST_CASE(dbwrapper_iterator)
{
    path ph = temp_directory_path() / unique_path();
    bool isObfuscated;
    std::unique_ptr<CDBWrapper> dbw(CreateLevelDB(ph, (1 << 20), isObfuscated, true, false));

    // The two keys are intentionally chosen for ordering
    char key = 'j';
    uint256 in = GetRandHash();
    BOOST_CHECK(dbw->Write(key, in));
    char key2 = 'k';
    uint256 in2 = GetRandHash();
    BOOST_CHECK(dbw->Write(key2, in2));

    boost::scoped_ptr<CDBIterator> it(const_cast<CDBWrapper*>(dbw.get())->NewIterator());

    // Be sure to seek past the obfuscation key (if it exists)
    // XT note: We don't support obfuscation keys.
    it->Seek(key);

    char key_res;
    uint256 val_res;

    it->GetKey(key_res);
    it->GetValue(val_res);
    BOOST_CHECK_EQUAL(key_res, key);
    BOOST_CHECK_EQUAL(val_res.ToString(), in.ToString());

    it->Next();

    it->GetKey(key_res);
    it->GetValue(val_res);
    BOOST_CHECK_EQUAL(key_res, key2);
    BOOST_CHECK_EQUAL(val_res.ToString(), in2.ToString());

    it->Next();
    BOOST_CHECK_EQUAL(it->Valid(), false);
}

BOOST_AUTO_TEST_CASE(readwrite_existing_data)
{
    // We're going to share this path between two wrappers
    path ph = temp_directory_path() / unique_path();
    create_directories(ph);

    // Set up a non-obfuscated wrapper to write some initial data.
    bool isObfuscated;
    std::unique_ptr<CDBWrapper> dbw(CreateLevelDB(ph, (1 << 10), isObfuscated, false, false));
    char key = 'k';
    uint256 in = GetRandHash();
    uint256 res;

    BOOST_CHECK(dbw->Write(key, in));
    BOOST_CHECK(dbw->Read(key, res));
    BOOST_CHECK_EQUAL(res.ToString(), in.ToString());

    // Call the destructor to free leveldb LOCK
    dbw.reset();

    // Now, set up another wrapper that wants to use the same directory
    std::unique_ptr<CDBWrapper> odbw(CreateLevelDB(ph, (1 << 10), isObfuscated, false, false));

    // Check that the key/val we wrote with original wrapper exists and
    // is readable.
    uint256 res2;
    BOOST_CHECK(odbw->Read(key, res2));
    BOOST_CHECK_EQUAL(res2.ToString(), in.ToString());

    BOOST_CHECK(!odbw->IsEmpty()); // There should be existing data

    uint256 in2 = GetRandHash();
    uint256 res3;

    // Check that we can write successfully
    BOOST_CHECK(odbw->Write(key, in2));
    BOOST_CHECK(odbw->Read(key, res3));
    BOOST_CHECK_EQUAL(res3.ToString(), in2.ToString());
}

BOOST_AUTO_TEST_CASE(existing_data_reindex)
{
    // We're going to share this path between two wrappers
    path ph = temp_directory_path() / unique_path();
    create_directories(ph);

    // Set up a non-obfuscated wrapper to write some initial data.
    bool isObfuscated;
    std::unique_ptr<CDBWrapper> dbw(CreateLevelDB(ph, (1 << 10), isObfuscated, false, false));
    char key = 'k';
    uint256 in = GetRandHash();
    uint256 res;

    BOOST_CHECK(dbw->Write(key, in));
    BOOST_CHECK(dbw->Read(key, res));
    BOOST_CHECK_EQUAL(res.ToString(), in.ToString());

    // Call the destructor to free leveldb LOCK
    dbw.reset();

    // Simulate a -reindex by wiping the existing data store
    std::unique_ptr<CDBWrapper> odbw(CreateLevelDB(ph, (1 << 10), isObfuscated, false, true));

    // Check that the key/val we wrote previously doesn't exist
    uint256 res2;
    BOOST_CHECK(!odbw->Read(key, res2));

    uint256 in2 = GetRandHash();
    uint256 res3;

    // Check that we can write successfully
    BOOST_CHECK(odbw->Write(key, in2));
    BOOST_CHECK(odbw->Read(key, res3));
    BOOST_CHECK_EQUAL(res3.ToString(), in2.ToString());
}

BOOST_AUTO_TEST_SUITE_END()
