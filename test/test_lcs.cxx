/// @b License: @n Apache License v2.0
/// @copyright Robert Dunnagan
#include <gtest/gtest.h>

#include <fmt/format.h>
#include <nodel/core/LCS.hxx>
#include <nodel/core.hxx>

using namespace nodel;
using namespace nodel::algo;

TEST(LCS, NoMatch) {
    String lhs = "abcd";
    String rhs = "efgh";
    LCS<String> lcs_search;
    String lcs;
    auto len = lcs_search.search(lhs, rhs, &lcs);
    ASSERT_EQ(len, 0UL);
    EXPECT_EQ(lcs, "");
}

TEST(LCS, Basic) {
    String lhs = "pAzBCD";
    String rhs = "qABxxCyD";
    LCS<String> lcs_search;
    String lcs;
    auto len = lcs_search.search(lhs, rhs, &lcs);
    ASSERT_EQ(len, 4UL);
    EXPECT_EQ(lcs, "ABCD");
}

TEST(LCS, BugFix) {
    String lhs = "xabcz";
    String rhs = "auvbxc";
    LCS<String> lcs_search;
    String lcs;
    auto len = lcs_search.search(lhs, rhs, &lcs);
    ASSERT_EQ(len, 3UL);
    EXPECT_EQ(lcs, "abc");
}

TEST(LCS, EmptyStrings) {
    String lhs = "";
    String rhs = "efgh";
    LCS<String> lcs_search;
    String lcs;
    auto len = lcs_search.search(lhs, rhs, &lcs);
    ASSERT_EQ(len, 0UL);
    EXPECT_EQ(lcs, "");

    lcs.clear();
    lhs = "abcd";
    rhs = "";
    len = lcs_search.search(lhs, rhs, &lcs);
    ASSERT_EQ(len, 0UL);
    EXPECT_EQ(lcs, "");

    lcs.clear();
    lhs = "";
    rhs = "";
    len = lcs_search.search(lhs, rhs, &lcs);
    ASSERT_EQ(len, 0UL);
    EXPECT_EQ(lcs, "");
}

TEST(LCS, ObjectList) {
    Object lhs = "['apple', 'banana', 'carrot', 'pear', 'orange']"_json;
    Object rhs = "['apricot', 'carrot', 'banana', 'orange', 'peach']"_json;
    LCS<ObjectList> lcs_search;
    ObjectList lcs;
    auto len = lcs_search.search(lhs.as<ObjectList>(), rhs.as<ObjectList>(), &lcs);
    ASSERT_EQ(len, 2UL);
    EXPECT_EQ(Object{lcs}, "['carrot', 'orange']"_json);
}

TEST(LCS, OrderedMap) {
    Object lhs = "{'x': 'X', 'a': 'A', 'b': 'B', 'c': 'C', 'z': 'Z'}"_json;
    Object rhs = "{'a': 'A', 'u': 'U', 'v': 'V', 'b': 'B', 'x': 'X', 'c': 'C'}"_json;

    LCS<KeyList> lcs_search;
    lcs_search.search(collect(lhs.iter_keys()), collect(rhs.iter_keys()));
    for (auto& indices : lcs_search) {
        DEBUG("{} {}", indices.first, indices.second);
    }
}
