// Copyright (c) 2017-2018 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "guiutiltests.h"
#include "receiverequestdialog.h"
#include "options.h"

void GUIUtilTests::toCurrentEncodingTest() {
#ifdef ENABLE_WALLET //< receiverequestdialog is not compiled unless this is defined
    auto arg = new DummyArgGetter;
    auto raii = SetDummyArgGetter(std::unique_ptr<ArgGetter>(arg));

    // garbage in, garbage out
    QVERIFY(ToCurrentEncoding("garbage") == "garbage");

    QString cashaddr_pubkey =
        "bitcoincash:qpm2qsznhks23z7629mms6s4cwef74vcwvy22gdx6a";
    QString base58_pubkey = "1BpEi6DfDAUFd7GtittLSdBeYJvcoaVggu";

    arg->Set("-usecashaddr", 1);
    QVERIFY(ToCurrentEncoding(cashaddr_pubkey) == cashaddr_pubkey);
    QVERIFY(ToCurrentEncoding(base58_pubkey) == cashaddr_pubkey);

    arg->Set("-usecashaddr", 0);
    QVERIFY(ToCurrentEncoding(cashaddr_pubkey) == base58_pubkey);
    QVERIFY(ToCurrentEncoding(base58_pubkey) == base58_pubkey);
#endif
}
