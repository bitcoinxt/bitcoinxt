// todo: Test merge
#include <boost/test/unit_test_suite.hpp>
#include <boost/test/test_tools.hpp>
#include "blockencodings.h"
#include "thinblock.h"
#include "uint256.h"

BOOST_AUTO_TEST_SUITE(thinblock_tests)

BOOST_AUTO_TEST_CASE(thintx_init) {
    ThinTx a = ThinTx::Null();
    BOOST_CHECK(!a.hasFull());
    BOOST_CHECK(!a.hasCheap());
    BOOST_CHECK(a.isNull());

    ThinTx b = ThinTx(uint256S("0xCBA"));
    BOOST_CHECK(b.hasFull());
    BOOST_CHECK(b.full() == uint256S("0xCBA"));
    BOOST_CHECK(b.hasCheap());
    BOOST_CHECK(b.cheap() == uint256S("0xCBA").GetCheapHash());
    BOOST_CHECK(!b.isNull());

    ThinTx c = ThinTx(GetShortID(0xabc, 0xbbc, uint256S("0xCBA")), 0xabc, 0xbcc);
    BOOST_CHECK(!c.hasFull());
    BOOST_CHECK(!c.hasCheap());
    BOOST_CHECK(!c.isNull());
}

BOOST_AUTO_TEST_CASE(thintx_equal) {

    ThinTx a = ThinTx::Null();
    ThinTx b = ThinTx::Null();
    ThinTx c = ThinTx(uint256S("0xCBA"));

    const uint64_t idk0_a = 0xf00;
    const uint64_t idk1_a = 0xbaa;

    // null
    BOOST_CHECK(a.equals(b));
    BOOST_CHECK(!a.equals(c));

    // same type vs same type
    a = b = ThinTx(uint256S("0xABC"));
    BOOST_CHECK(a.equals(b));
    BOOST_CHECK(!a.equals(c));

    a = b = ThinTx(uint256S("0xABC").GetCheapHash());
    BOOST_CHECK(a.equals(b));
    BOOST_CHECK(!a.equals(ThinTx(c.cheap())));

    a = b = ThinTx(GetShortID(
        idk0_a, idk1_a, uint256S("0xABC")),
        idk0_a, idk1_a);

    BOOST_CHECK(a.equals(b));
    c = ThinTx(GetShortID(
        0xfefe, 0xbaba, uint256S("0xABC")),
        0xfefe, 0xbaba);
    BOOST_CHECK(!a.equals(c));
    BOOST_CHECK(!c.equals(a));
    c = ThinTx(GetShortID(
        idk0_a, idk1_a, uint256S("0xCBA")),
        idk0_a, idk1_a);
    BOOST_CHECK(!c.equals(a));


    // full vs cheap
    a = ThinTx(uint256S("0xABC"));
    b = ThinTx(uint256S("0xABC").GetCheapHash());
    BOOST_CHECK(a.equals(b));
    BOOST_CHECK(b.equals(a));
    b = ThinTx(uint256S("0xCBA").GetCheapHash());
    BOOST_CHECK(!a.equals(b));
    BOOST_CHECK(!b.equals(a));

    // full vs obfuscated
    a = ThinTx(GetShortID(
        0xfefe, 0xbaba, uint256S("0xABC")),
        0xfefe, 0xbaba);
    b = ThinTx(uint256S("0xABC"));
    BOOST_CHECK(a.equals(b));
}

BOOST_AUTO_TEST_SUITE_END()
