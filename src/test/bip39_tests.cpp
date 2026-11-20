// Copyright (c) 2026-present The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <util/nfkd.h>

#include <boost/test/unit_test.hpp>

#include <string>

BOOST_AUTO_TEST_SUITE(bip39_tests)

BOOST_AUTO_TEST_CASE(nfkd_basics)
{
    // ASCII is not affected by NFKD.
    BOOST_CHECK_EQUAL(NFKD(""), std::string(""));
    BOOST_CHECK_EQUAL(NFKD("SATOSHI"), std::string("SATOSHI"));
    BOOST_CHECK_EQUAL(NFKD("legal winner thank year"), std::string("legal winner thank year"));

    // Invalid UTF-8 is returned unchanged.
    BOOST_CHECK_EQUAL(NFKD(std::string("\xff")), std::string("\xff"));
}

BOOST_AUTO_TEST_CASE(nfkd_canonical_equivalences)
{
    // The precomposed (U+00E9) and combining (U+0065 U+0301) spellings of
    // the last letter of "caf\u{e9}" normalize to the same string.
    BOOST_CHECK_EQUAL(NFKD("caf\xc3\xa9"), std::string("cafe\xcc\x81"));
    BOOST_CHECK_EQUAL(NFKD("cafe\xcc\x81"), std::string("cafe\xcc\x81"));

    // Decomposition mappings are applied recursively (U+212B -> U+00C5 ->
    // "A" + U+030A).
    BOOST_CHECK_EQUAL(NFKD("\xe2\x84\xab"), std::string("A\xcc\x8a"));

    // Combining marks are sorted by canonical combining class (U+0301 has
    // class 230, U+0316 has class 220).
    BOOST_CHECK_EQUAL(NFKD(std::string("A\xcc\x81\xcc\x96")), std::string("A\xcc\x96\xcc\x81"));
}

BOOST_AUTO_TEST_CASE(nfkd_compatibility_equivalences)
{
    // Fullwidth A and B (U+FF21, U+FF22).
    BOOST_CHECK_EQUAL(NFKD("\xef\xbc\xa1\xef\xbc\xa2"), std::string("AB"));
    // Superscript two (U+00B2).
    BOOST_CHECK_EQUAL(NFKD("\xc2\xb2"), std::string("2"));
    // Latin small ligature fi (U+FB01).
    BOOST_CHECK_EQUAL(NFKD("\xef\xac\x81"), std::string("fi"));
    // Arabic ligature with the longest (18 character) decomposition (U+FDFA).
    BOOST_CHECK_EQUAL(NFKD("\xef\xb7\xba"),
                      std::string("\xd8\xb5\xd9\x84\xd9\x89\x20\xd8\xa7\xd9\x84\xd9\x84\xd9\x87\x20"
                                  "\xd8\xb9\xd9\x84\xd9\x8a\xd9\x87\x20\xd9\x88\xd8\xb3\xd9\x84\xd9\x85"));
    // Hangul syllable U+AC01 decomposes algorithmically.
    BOOST_CHECK_EQUAL(NFKD("\xea\xb0\x81"), std::string("\xe1\x84\x80\xe1\x85\xa1\xe1\x86\xa8"));
}

BOOST_AUTO_TEST_SUITE_END()
