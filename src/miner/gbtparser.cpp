#include "miner/gbtparser.h"

#include "core_io.h"
#include "miner/utilminer.h"
#include "primitives/block.h"
#include "primitives/transaction.h"
#include "utildebug.h"
#include "utilstrencodings.h"

#include <univalue.h>

namespace miner {

template <class T>
T get(const UniValue& root, const std::string& key) {
    UniValue value = find_value(root, key);
    if (value.isNull()) {
        throw std::invalid_argument("Could not find key '"
                                    + key + "' in json object");
    }
    return value;
}

template <>
int64_t get<int64_t>(const UniValue& root, const std::string& key) {
    return get<UniValue>(root, key).get_int64();
}

template <>
int get<int>(const UniValue& root, const std::string& key) {
    return get<UniValue>(root, key).get_int();
}

template <>
std::string get<std::string>(const UniValue& root, const std::string& key) {
    return get<UniValue>(root, key).get_str();
}

CBlock ParseGBT(const UniValue& gbt) {

    CBlock parsed;

    parsed.nVersion = get<int64_t>(gbt, "version");
    parsed.hashPrevBlock = uint256S(get<std::string>(gbt, "previousblockhash"));
    parsed.nTime = get<int64_t>(gbt, "curtime");

    const std::string bitsHex = get<std::string>(gbt, "bits");
    std::stringstream ss;
    ss << std::hex << bitsHex;
    ss >> parsed.nBits;

    // Create coinbase
    CMutableTransaction coinbase;
    {
        UniValue coinbaseaux = get<UniValue>(gbt, "coinbaseaux");
        std::vector<uint8_t> flags = ParseHex(get<std::string>(coinbaseaux, "flags"));

        coinbase.vin.resize(1);
        coinbase.vin[0].scriptSig = CScript() << get<int>(gbt, "height") << flags;
        coinbase.vin[0].prevout.SetNull();
        coinbase.vout.resize(1);
        coinbase.vout[0].scriptPubKey = CScript() << OP_TRUE;
        coinbase.vout[0].nValue = get<int64_t>(gbt, "coinbasevalue");
        BloatCoinbaseSize(coinbase);
    }
    parsed.vtx.push_back(CTransaction(coinbase));

    UniValue txs = get<UniValue>(gbt, "transactions");
    for (auto tx : txs.getValues()) {
        parsed.vtx.push_back(DecodeHexTx(get<std::string>(tx, "data")));
    }
    return parsed;
}

} // ns miner
