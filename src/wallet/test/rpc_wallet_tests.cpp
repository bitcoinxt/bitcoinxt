// Copyright (c) 2013-2014 The Bitcoin Core developers
// Copyright (c) 2018 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "rpc/server.h"
#include "rpc/client.h"

#include "dstencode.h"
#include "main.h"
#include "wallet/wallet.h"

#include "wallet/test/wallet_test_fixture.h"

#include <boost/algorithm/string.hpp>
#include <boost/test/unit_test.hpp>

#include <univalue.h>

using namespace std;

JSONRPCRequest createArgs(int nRequired, const char* address1=NULL, const char* address2=NULL)
{
    UniValue params(UniValue::VARR);
    params.push_back(nRequired);
    UniValue addresses(UniValue::VARR);
    if (address1) addresses.push_back(address1);
    if (address2) addresses.push_back(address2);
    params.push_back(addresses);

    JSONRPCRequest req;
    req.params = params;
    req.fHelp = false;
    return req;
}

extern UniValue CallRPC(string args);

extern CWallet* pwalletMain;

BOOST_FIXTURE_TEST_SUITE(rpc_wallet_tests, WalletTestingSetup)

BOOST_AUTO_TEST_CASE(rpc_addmultisig)
{
    LOCK(pwalletMain->cs_wallet);

    rpcfn_type addmultisig = tableRPC["addmultisigaddress"]->actor;

    // old, 65-byte-long:
    const char address1Hex[] = "0434e3e09f49ea168c5bbf53f877ff4206923858aab7c7e1df25bc263978107c95e35065a27ef6f1b27222db0ec97e0e895eaca603d3ee0d4c060ce3d8a00286c8";
    // new, compressed:
    const char address2Hex[] = "0388c2037017c62240b6b72ac1a2a5f94da790596ebd06177c8572752922165cb4";

    UniValue v;
    BOOST_CHECK_NO_THROW(v = addmultisig(createArgs(1, address1Hex)));
    BOOST_CHECK(IsValidDestinationString(v.get_str()));

    BOOST_CHECK_NO_THROW(v = addmultisig(createArgs(1, address1Hex, address2Hex)));
    BOOST_CHECK(IsValidDestinationString(v.get_str()));

    BOOST_CHECK_NO_THROW(v = addmultisig(createArgs(2, address1Hex, address2Hex)));
    BOOST_CHECK(IsValidDestinationString(v.get_str()));

    BOOST_CHECK_THROW(addmultisig(createArgs(0)), runtime_error);
    BOOST_CHECK_THROW(addmultisig(createArgs(1)), runtime_error);
    BOOST_CHECK_THROW(addmultisig(createArgs(2, address1Hex)), runtime_error);

    BOOST_CHECK_THROW(addmultisig(createArgs(1, "")), runtime_error);
    BOOST_CHECK_THROW(addmultisig(createArgs(1, "NotAValidPubkey")), runtime_error);

    string short1(address1Hex, address1Hex + sizeof(address1Hex) - 2); // last byte missing
    BOOST_CHECK_THROW(addmultisig(createArgs(2, short1.c_str())), runtime_error);

    string short2(address1Hex + 1, address1Hex + sizeof(address1Hex)); // first byte missing
    BOOST_CHECK_THROW(addmultisig(createArgs(2, short2.c_str())), runtime_error);
}

BOOST_AUTO_TEST_CASE(rpc_wallet)
{
    // Test RPC calls for various wallet statistics
    UniValue r;

    LOCK2(cs_main, pwalletMain->cs_wallet);

    CPubKey demoPubkey = pwalletMain->GenerateNewKey();
    CTxDestination dest(demoPubkey.GetID());
    std::string destStr = EncodeDestination(dest);
    UniValue retValue;
    string strAccount = "walletDemoAccount";
    string strPurpose = "receive";
    BOOST_CHECK_NO_THROW({ /*Initialize Wallet with an account */
        CWalletDB walletdb(pwalletMain->strWalletFile);
        CAccount account;
        account.vchPubKey = demoPubkey;
        pwalletMain->SetAddressBook(account.vchPubKey.GetID(), strAccount, strPurpose);
        walletdb.WriteAccount(strAccount, account);
    });

    CPubKey setaccountDemoPubkey = pwalletMain->GenerateNewKey();
    std::string setaccountDemoAddress = EncodeDestination(CTxDestination(setaccountDemoPubkey.GetID()));

    /*********************************
     * 			setaccount
     *********************************/
    BOOST_CHECK_NO_THROW(CallRPC("setaccount " + setaccountDemoAddress + " nullaccount"));
    /* 1D1ZrZNe3JUo7ZycKEYQQiQAWd9y54F4XZ is not owned by the test wallet. */
    BOOST_CHECK_THROW(CallRPC("setaccount 1D1ZrZNe3JUo7ZycKEYQQiQAWd9y54F4XZ nullaccount"), runtime_error);
    BOOST_CHECK_THROW(CallRPC("setaccount"), runtime_error);
    /* 1D1ZrZNe3JUo7ZycKEYQQiQAWd9y54F4X (33 chars) is an illegal address (should be 34 chars) */
    BOOST_CHECK_THROW(CallRPC("setaccount 1D1ZrZNe3JUo7ZycKEYQQiQAWd9y54F4X nullaccount"), runtime_error);


    /*********************************
     *                  getbalance
     *********************************/
    BOOST_CHECK_NO_THROW(CallRPC("getbalance"));
    BOOST_CHECK_NO_THROW(CallRPC("getbalance " + destStr));

    /*********************************
     * 			listunspent
     *********************************/
    BOOST_CHECK_NO_THROW(CallRPC("listunspent"));
    BOOST_CHECK_THROW(CallRPC("listunspent string"), runtime_error);
    BOOST_CHECK_THROW(CallRPC("listunspent 0 string"), runtime_error);
    BOOST_CHECK_THROW(CallRPC("listunspent 0 1 not_array"), runtime_error);
    BOOST_CHECK_THROW(CallRPC("listunspent 0 1 [] extra"), runtime_error);
    BOOST_CHECK_NO_THROW(r = CallRPC("listunspent 0 1 []"));
    BOOST_CHECK(r.get_array().empty());

    /*********************************
     * 		listreceivedbyaddress
     *********************************/
    BOOST_CHECK_NO_THROW(CallRPC("listreceivedbyaddress"));
    BOOST_CHECK_NO_THROW(CallRPC("listreceivedbyaddress 0"));
    BOOST_CHECK_THROW(CallRPC("listreceivedbyaddress not_int"), runtime_error);
    BOOST_CHECK_THROW(CallRPC("listreceivedbyaddress 0 not_bool"), runtime_error);
    BOOST_CHECK_NO_THROW(CallRPC("listreceivedbyaddress 0 true"));
    BOOST_CHECK_THROW(CallRPC("listreceivedbyaddress 0 true extra"), runtime_error);

    /*********************************
     * 		listreceivedbyaccount
     *********************************/
    BOOST_CHECK_NO_THROW(CallRPC("listreceivedbyaccount"));
    BOOST_CHECK_NO_THROW(CallRPC("listreceivedbyaccount 0"));
    BOOST_CHECK_THROW(CallRPC("listreceivedbyaccount not_int"), runtime_error);
    BOOST_CHECK_THROW(CallRPC("listreceivedbyaccount 0 not_bool"), runtime_error);
    BOOST_CHECK_NO_THROW(CallRPC("listreceivedbyaccount 0 true"));
    BOOST_CHECK_THROW(CallRPC("listreceivedbyaccount 0 true extra"), runtime_error);

    /*********************************
     *          listsinceblock
     *********************************/
    BOOST_CHECK_NO_THROW(CallRPC("listsinceblock"));

    /*********************************
     *          listtransactions
     *********************************/
    BOOST_CHECK_NO_THROW(CallRPC("listtransactions"));
    BOOST_CHECK_NO_THROW(CallRPC("listtransactions " + destStr));
    BOOST_CHECK_NO_THROW(CallRPC("listtransactions " + destStr + " 20"));
    BOOST_CHECK_NO_THROW(CallRPC("listtransactions " + destStr + " 20 0"));
    BOOST_CHECK_THROW(CallRPC("listtransactions " + destStr + " not_int"), runtime_error);

    /*********************************
     *          listlockunspent
     *********************************/
    BOOST_CHECK_NO_THROW(CallRPC("listlockunspent"));

    /*********************************
     *          listaccounts
     *********************************/
    BOOST_CHECK_NO_THROW(CallRPC("listaccounts"));

    /*********************************
     *          listaddressgroupings
     *********************************/
    BOOST_CHECK_NO_THROW(CallRPC("listaddressgroupings"));

    /*********************************
     * 		getrawchangeaddress
     *********************************/
    BOOST_CHECK_NO_THROW(CallRPC("getrawchangeaddress"));

    /*********************************
     * 		getnewaddress
     *********************************/
    BOOST_CHECK_NO_THROW(CallRPC("getnewaddress"));
    BOOST_CHECK_NO_THROW(CallRPC("getnewaddress getnewaddress_demoaccount"));

    /*********************************
     * 		getaccountaddress
     *********************************/
    BOOST_CHECK_NO_THROW(CallRPC("getaccountaddress \"\""));
    BOOST_CHECK_NO_THROW(CallRPC("getaccountaddress accountThatDoesntExists")); // Should generate a new account
    BOOST_CHECK_NO_THROW(retValue = CallRPC("getaccountaddress " + strAccount));
    BOOST_CHECK(DecodeDestination(retValue.get_str()) == dest);

    /*********************************
     * 			getaccount
     *********************************/
    BOOST_CHECK_THROW(CallRPC("getaccount"), runtime_error);
    BOOST_CHECK_NO_THROW(CallRPC("getaccount " + destStr));

    /*********************************
     * 	signmessage + verifymessage
     *********************************/
    BOOST_CHECK_NO_THROW(retValue = CallRPC("signmessage " + destStr + " mymessage"));
    BOOST_CHECK_THROW(CallRPC("signmessage"), runtime_error);
    /* Should throw error because this address is not loaded in the wallet */
    BOOST_CHECK_THROW(CallRPC("signmessage 1QFqqMUD55ZV3PJEJZtaKCsQmjLT6JkjvJ mymessage"), runtime_error);

    /* missing arguments */
    BOOST_CHECK_THROW(CallRPC("verifymessage " + destStr), runtime_error);
    BOOST_CHECK_THROW(CallRPC("verifymessage " + destStr + " " + retValue.get_str()), runtime_error);
    /* Illegal address */
    BOOST_CHECK_THROW(CallRPC("verifymessage 1D1ZrZNe3JUo7ZycKEYQQiQAWd9y54F4X " + retValue.get_str() + " mymessage"), runtime_error);
    /* wrong address */
    BOOST_CHECK(CallRPC("verifymessage 1D1ZrZNe3JUo7ZycKEYQQiQAWd9y54F4XZ " + retValue.get_str() + " mymessage").get_bool() == false);
    /* Correct address and signature but wrong message */
    BOOST_CHECK(CallRPC("verifymessage " + destStr + " " + retValue.get_str() + " wrongmessage").get_bool() == false);
    /* Correct address, message and signature*/
    BOOST_CHECK(CallRPC("verifymessage " + destStr + " " + retValue.get_str() + " mymessage").get_bool() == true);

    /*********************************
     * 		getaddressesbyaccount
     *********************************/
    BOOST_CHECK_THROW(CallRPC("getaddressesbyaccount"), runtime_error);
    BOOST_CHECK_NO_THROW(retValue = CallRPC("getaddressesbyaccount " + strAccount));
    UniValue arr = retValue.get_array();
    BOOST_CHECK(arr.size() > 0);
    BOOST_CHECK(DecodeDestination(arr[0].get_str()) == dest);

    /*********************************
     * 	     fundrawtransaction
     *********************************/
    BOOST_CHECK_THROW(CallRPC("fundrawtransaction 28z"), runtime_error);
    BOOST_CHECK_THROW(CallRPC("fundrawtransaction 01000000000180969800000000001976a91450ce0a4b0ee0ddeb633da85199728b940ac3fe9488ac00000000"), runtime_error);
}

BOOST_AUTO_TEST_SUITE_END()
