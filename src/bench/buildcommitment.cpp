#include "bench.h"
#include "random.h"
#include "coins.h"
#include "utilutxocommit.h"
#include "utxocommit.h"

namespace {
class DummyCoinsViewCursor : public CCoinsViewCursor {
public:
    DummyCoinsViewCursor(benchmark::State& state)
        : CCoinsViewCursor(uint256S("0xf00d")), state(state) {

        FastRandomContext r(true);

        for (int i = 0; i < 5000; ++i) {
            outpoints.push_back(COutPoint(uint256(r.randbytes(32)), r.rand32()));
            std::vector<uint8_t> script = r.randbytes(r.randrange(0x3f));
            coins.push_back(Coin(CTxOut(CAmount(r.randrange(1000)), CScript(script)),
                                 r.randrange(10000), r.randrange(1)));
        }
        currOutpoint = begin(outpoints);
        currCoin = begin(coins);
    }

    bool GetKey(COutPoint &key) const override {
        key = *currOutpoint;
        return true;
    }

    bool GetValue(Coin &coin) const override {
        coin = *currCoin;
        return true;
    }
    unsigned int GetValueSize() const override {
        assert(!"not implemented");
    }

    bool Valid() const override {
        return state.KeepRunning();
    }
    void Next() {
        if (++currOutpoint == end(outpoints))
            currOutpoint = begin(outpoints);
        if (++currCoin == end(coins))
            currCoin = begin(coins);
    }

private:
    benchmark::State& state;

    // Dummy data that we loop through.
    std::vector<COutPoint> outpoints;
    std::vector<COutPoint>::iterator currOutpoint;
    std::vector<Coin> coins;
    std::vector<Coin>::iterator currCoin;
};

} // ns anon

static void BuildCommitment4(benchmark::State& state) {
    DummyCoinsViewCursor cursor(state);
    BuildUtxoCommit(&cursor, 4);
}
static void BuildCommitment8(benchmark::State& state) {
    DummyCoinsViewCursor cursor(state);
    BuildUtxoCommit(&cursor, 8);
}
static void BuildCommitment16(benchmark::State& state) {
    DummyCoinsViewCursor cursor(state);
    BuildUtxoCommit(&cursor, 16);
}

BENCHMARK(BuildCommitment4);
BENCHMARK(BuildCommitment8);
BENCHMARK(BuildCommitment16);
