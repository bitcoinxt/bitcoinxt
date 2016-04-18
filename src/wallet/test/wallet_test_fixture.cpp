#include "wallet/test/wallet_test_fixture.h"

#include "rpcserver.h"
#include "wallet/db.h"
#include "wallet/wallet.h"
#include "init.h" // pwalletMain

WalletTestingSetup::WalletTestingSetup(const std::string& chainName):
    TestingSetup(chainName)
{
    bitdb.MakeMock();

    bool fFirstRun;
    pwalletMain = new CWallet("wallet.dat");
    pwalletMain->LoadWallet(fFirstRun);
    RegisterValidationInterface(pwalletMain);

    // RegisterWalletRPCCommands(tableRPC);
    walletRegisterRPCCommands();
}

WalletTestingSetup::~WalletTestingSetup()
{
    UnregisterValidationInterface(pwalletMain);
    delete pwalletMain;
    pwalletMain = NULL;

    bitdb.Flush(true);
    bitdb.Reset();
}
