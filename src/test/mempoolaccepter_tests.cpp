#include "test/test_bitcoin.h"
#include "test/test_random.h"
#include "coins.h"
#include "mempoolaccepter.h"
#include "policy/txpriority.h"

#include <boost/test/unit_test.hpp>

namespace {
CMutableTransaction CreateDummyTx() {
    CScript garbage;
    for (unsigned int i = 0; i < 128; i++) {
        // use rand to get unique hash
        garbage.push_back(static_cast<char>(insecure_rand()));
    }

    CMutableTransaction tx;
    tx.vin.resize(1);
    tx.vin[0].scriptSig = garbage;
    tx.vout.resize(1);
    tx.vout[0].nValue=0LL;
    return tx;
}

size_t TxSize(const CMutableTransaction& tx) {
    return ::GetSerializeSize(CTransaction(tx), SER_NETWORK, PROTOCOL_VERSION);
}

class DummyFeeEvaluator : public FeeEvaluator {
public:
    using FeeEvaluator::FeeEvaluator;

    double GetPriority(const CCoinsViewCache& view,
                        const CTxMemPoolEntry& entry,
                        int tipHeight) const override
    {
        return priority;
    }
    double priority;
};

class MempoolAccepterTestFixture : public BasicTestingSetup {
public:
    MempoolAccepterTestFixture() : minRelayFeeRate(1000), allowFreeTxs(true), view(&dummybase)
    {
    }

    MempoolFeeModifier feemodifier;
    CFeeRate minRelayFeeRate;
    bool allowFreeTxs;
    CCoinsView dummybase;
    CCoinsViewCache view;

    CTxMemPoolEntry CreateTxWithFee(bool absurdFee = false)
    {
        CMutableTransaction tx = CreateDummyTx();
        CAmount fee = minRelayFeeRate.GetFee(TxSize(tx));
        if (absurdFee) {
            fee *= ABSURD_FEE_FACTOR;
        }
        TestMemPoolEntryHelper entry;
        return entry.Fee(fee).Time(GetTime()).Height(0).FromTx(tx, nullptr);
    }

    CTxMemPoolEntry CreateFreeTx()
    {
        CMutableTransaction tx = CreateDummyTx();
        TestMemPoolEntryHelper entry;
        return entry.Fee(CAmount(0)).Time(GetTime()).Height(0).FromTx(tx, nullptr);
    }

    FeeEvaluator FeeEval() {
        static RateLimitState nolimit(std::numeric_limits<int64_t>::max());
        return FeeEvaluator(allowFreeTxs, feemodifier, minRelayFeeRate, &nolimit);
    }
};
} // ns anon

BOOST_FIXTURE_TEST_SUITE(mempoolaccepter_tests, MempoolAccepterTestFixture)

BOOST_AUTO_TEST_CASE(hassufficientfee_has_delta) {
    allowFreeTxs = false;

    // No delta = not sufficient.
    CTxMemPoolEntry freetx = CreateFreeTx();
    BOOST_CHECK_EQUAL(FeeEvaluator::INSUFFICIENT_FEE,
                      FeeEval().HasSufficientFee(view, freetx, 0));

    // Transactions that are prioritized never need any relay fee.
    // Positive delta = prioritized
    feemodifier.AddDelta(freetx.GetTx().GetHash(), 1);
    BOOST_CHECK_EQUAL(FeeEvaluator::FEE_OK,
                      FeeEval().HasSufficientFee(view, freetx, 0));

    // TXs with negative delta require normal relay fee.
    feemodifier.AddDelta(freetx.GetTx().GetHash(), -2);
    BOOST_CHECK_EQUAL(FeeEvaluator::INSUFFICIENT_FEE,
                      FeeEval().HasSufficientFee(view, freetx, 0));

    CTxMemPoolEntry txwithfee = CreateTxWithFee();
    feemodifier.AddDelta(txwithfee.GetTx().GetHash(), -2);
    BOOST_CHECK_EQUAL(FeeEvaluator::FEE_OK,
                      FeeEval().HasSufficientFee(view, txwithfee, 0));
}

BOOST_AUTO_TEST_CASE(hassufficientfee_not_free) {
    allowFreeTxs = false;
    BOOST_CHECK_EQUAL(FeeEvaluator::FEE_OK,
                      FeeEval().HasSufficientFee(view, CreateTxWithFee(), 0));
    BOOST_CHECK_EQUAL(FeeEvaluator::INSUFFICIENT_FEE,
                      FeeEval().HasSufficientFee(view, CreateFreeTx(), 0));
}

BOOST_AUTO_TEST_CASE(getminrelayfee_free) {
    allowFreeTxs = true;
    CTxMemPoolEntry freetx = CreateFreeTx();
    DummyFeeEvaluator eval(allowFreeTxs, feemodifier, minRelayFeeRate);
    eval.priority = AllowFreeThreshold();
    BOOST_CHECK_EQUAL(FeeEvaluator::FEE_OK, eval.HasSufficientFee(view, freetx, 0));
    eval.priority = AllowFreeThreshold() - 1;
    BOOST_CHECK_EQUAL(FeeEvaluator::INSUFFICIENT_PRIORITY, eval.HasSufficientFee(view, freetx, 0));
}

BOOST_AUTO_TEST_CASE(getminrelayfree_rate_limited) {
    allowFreeTxs = true;
    CTxMemPoolEntry freetx = CreateFreeTx();
    RateLimitState limit(0 /* limit will always exceed */);
    DummyFeeEvaluator eval(allowFreeTxs, feemodifier, minRelayFeeRate, &limit);
    eval.priority = AllowFreeThreshold();
    BOOST_CHECK_EQUAL(FeeEvaluator::RATE_LIMITED, eval.HasSufficientFee(view, freetx, 0));
}

BOOST_AUTO_TEST_CASE(absurdfee) {
    bool absurdHighFee = true;
    CTxMemPoolEntry tx = CreateTxWithFee(absurdHighFee);
    {
        allowFreeTxs = true;
        DummyFeeEvaluator eval(allowFreeTxs, feemodifier, minRelayFeeRate);
        BOOST_CHECK_EQUAL(FeeEvaluator::ABSURD_HIGH_FEE,
                          eval.HasSufficientFee(view, tx, 0));
    }
    {
        allowFreeTxs = false;
        DummyFeeEvaluator eval(allowFreeTxs, feemodifier, minRelayFeeRate);
        BOOST_CHECK_EQUAL(FeeEvaluator::ABSURD_HIGH_FEE,
                          eval.HasSufficientFee(view, tx, 0));
    }

}

BOOST_AUTO_TEST_SUITE_END()
