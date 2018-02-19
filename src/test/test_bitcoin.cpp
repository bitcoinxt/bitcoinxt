// Copyright (c) 2011-2013 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#define BOOST_TEST_MODULE Bitcoin Test Suite

#include "test_bitcoin.h"

#include "key.h"
#include "main.h"
#include "random.h"
#include "txdb.h"
#include "txmempool.h"
#include "ui_interface.h"
#include "options.h"

#include "test/testutil.h"

#include <boost/filesystem.hpp>
#include <boost/test/unit_test.hpp>
#include <boost/thread.hpp>

CClientUIInterface uiInterface; // Declared but not defined in ui_interface.h
CWallet* pwalletMain;
FastRandomContext insecure_rand_ctx(true);
std::unique_ptr<CConnman> g_connman;

extern bool fPrintToConsole;
extern void noui_connect();

BasicTestingSetup::BasicTestingSetup(const std::string& chainName)
{
        ECC_Start();
        SetupEnvironment();
        fPrintToDebugLog = false; // don't want to write to debug.log file
        fCheckBlockIndex = true;
        SelectParams(chainName);
}
BasicTestingSetup::~BasicTestingSetup()
{
        ECC_Stop();
        g_connman.reset();
}

TestingSetup::TestingSetup(const std::string& chainName) : BasicTestingSetup(chainName)
{
        ClearDatadirCache();
        pathTemp = GetTempPath() / strprintf("test_bitcoin_%lu_%i", (unsigned long)GetTime(), (int)(GetRand(100000)));
        boost::filesystem::create_directories(pathTemp);
        mapArgs["-datadir"] = pathTemp.string();
        mempool.setSanityCheck(1.0);
        bool isObfuscated;
        pblocktree = new CBlockTreeDB(1 << 20, isObfuscated, true);
        pcoinsdbview = new CCoinsViewDB(1 << 23, isObfuscated, true);
        pcoinsTip = new CCoinsViewCache(pcoinsdbview);
        InitBlockIndex();
        mapArgs["-par"] = "3";
        for (int i=0; i < Opt().ScriptCheckThreads()-1; i++)
            threadGroup.create_thread(&ThreadScriptCheck);
        g_connman = std::unique_ptr<CConnman>(new CConnman());
        connman = g_connman.get();
        RegisterNodeSignals(GetNodeSignals());
}

TestingSetup::~TestingSetup()
{
        UnregisterNodeSignals(GetNodeSignals());
        threadGroup.interrupt_all();
        threadGroup.join_all();
        UnloadBlockIndex();
        delete pcoinsTip;
        delete pcoinsdbview;
        delete pblocktree;
        boost::filesystem::remove_all(pathTemp);
}

CTxMemPoolEntry TestMemPoolEntryHelper::FromTx(CMutableTransaction &tx, CTxMemPool *pool) {
    CTransaction txn(tx);
    return FromTx(txn, pool);
}

CTxMemPoolEntry TestMemPoolEntryHelper::FromTx(CTransaction &txn, CTxMemPool *pool) {
    bool hasNoDependencies = pool ? pool->HasNoInputsOf(txn) : hadNoDependencies;

    return CTxMemPoolEntry(txn, nFee, nTime, dPriority, nHeight,
                           hasNoDependencies, spendsCoinbase, lp, sigOpCount);
}

void Shutdown(void* parg)
{
  exit(0);
}

void StartShutdown()
{
  exit(0);
}

bool ShutdownRequested()
{
  return false;
}
