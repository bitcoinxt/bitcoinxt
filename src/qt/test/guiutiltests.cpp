// Copyright (c) 2017-2018 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "guiutiltests.h"
#include "receiverequestdialog.h"
#include "options.h"

namespace {
class DummyArgGetter : public ArgGetter {
    public:
        DummyArgGetter() : ArgGetter(), useCashAddr(false) { }
        int64_t GetArg(const std::string& strArg, int64_t def) override {
            return def;
        }
        std::vector<std::string> GetMultiArgs(const std::string& arg) override {
            assert(false);
        }
        bool GetBool(const std::string& arg, bool def) override {
            return arg == "-usecashaddr" ? useCashAddr : def;
        }
        bool useCashAddr;
};
} // ns anon

void GUIUtilTests::toCurrentEncodingTest() {
#ifdef ENABLE_WALLET //< receiverequestdialog is not compiled unless this is defined
    auto arg = new DummyArgGetter;
    auto raii = SetDummyArgGetter(std::unique_ptr<ArgGetter>(arg));

    // garbage in, garbage out
    QVERIFY(ToCurrentEncoding("garbage") == "garbage");

    QString cashaddr_pubkey =
        "bitcoincash:qpm2qsznhks23z7629mms6s4cwef74vcwvy22gdx6a";
    QString base58_pubkey = "1BpEi6DfDAUFd7GtittLSdBeYJvcoaVggu";

    arg->useCashAddr = true;
    QVERIFY(ToCurrentEncoding(cashaddr_pubkey) == cashaddr_pubkey);
    QVERIFY(ToCurrentEncoding(base58_pubkey) == cashaddr_pubkey);

    arg->useCashAddr = false;
    QVERIFY(ToCurrentEncoding(cashaddr_pubkey) == base58_pubkey);
    QVERIFY(ToCurrentEncoding(base58_pubkey) == base58_pubkey);
#endif
}
