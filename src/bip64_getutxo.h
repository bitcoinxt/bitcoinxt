#ifndef BITCOIN_BIP64_GETUTXOS_H
#define BITCOIN_BIP64_GETUTXOS_H

#include "primitives/transaction.h"
#include "serialize.h"
#include "coins.h"

#include <cstdint>
#include <vector>
#include <string>

class CTxMemPool;

namespace bip64 {

struct CCoin {
    uint32_t nHeight;
    CTxOut out;

    ADD_SERIALIZE_METHODS;

    CCoin() : nHeight(0) {}
    CCoin(Coin&& in) : nHeight(in.nHeight), out(std::move(in.out)) {}

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        uint32_t nTxVerDummy = 0;
        READWRITE(nTxVerDummy);
        READWRITE(nHeight);
        READWRITE(out);
    }
};

class UTXORetriever {
public:
    UTXORetriever(const std::vector<COutPoint>& o,
                  CCoinsView& viewChain,
                  CTxMemPool* mempool);

    //! An array of bytes encoding one bit for each outpoint queried. Each bit
    //! indicates whether the queried outpoint was found in the UTXO set or not.
    std::vector<uint8_t> GetBitmap() const;
    //! String representation for GetBitmap (human-readable for json output)
    std::string GetBitmapStr() const;
    //! Result objects as defined in BIP64
    const std::vector<CCoin>& GetResults() const { return outs; }

private:
    // input
    const std::vector<COutPoint> outpoints;

    // output
    std::vector<bool> hits;
    std::vector<CCoin> outs;

    void Process(CCoinsView*, CTxMemPool*);
};

} // ns bip64

#endif // BITCOIN_BIP64_GETUTXOS_H
