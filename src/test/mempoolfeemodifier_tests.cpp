#include "test/test_bitcoin.h"
#include "mempoolfeemodifier.h"
#include "uint256.h"

#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(mempoolfeemodifier_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(modify_fee_delta)
{
    uint256 tx(uint256S("0xfee"));
    uint256 tx2(uint256S("0xfaa"));

    MempoolFeeModifier fee;
    BOOST_CHECK_EQUAL(CAmount(0), fee.GetDelta(tx));

    fee.AddDelta(tx, CAmount(1000));
    BOOST_CHECK_EQUAL(CAmount(1000), fee.GetDelta(tx));

    fee.AddDelta(tx, CAmount(1000));
    BOOST_CHECK_EQUAL(CAmount(2000), fee.GetDelta(tx));

    fee.AddDelta(tx2, CAmount(1000));
    BOOST_CHECK_EQUAL(CAmount(2000), fee.GetDelta(tx));
    BOOST_CHECK_EQUAL(CAmount(1000), fee.GetDelta(tx2));

    fee.RemoveDelta(tx2);
    BOOST_CHECK_EQUAL(CAmount(2000), fee.GetDelta(tx));
    BOOST_CHECK_EQUAL(CAmount(0), fee.GetDelta(tx2));
}

BOOST_AUTO_TEST_SUITE_END()
