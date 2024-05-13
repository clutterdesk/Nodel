/// @b License: @n Apache License v2.0
/// @copyright Robert Dunnagan
#include <gtest/gtest.h>

#include <fmt/format.h>
#include <nodel/core/LCS.h>
#include <nodel/core.h>

using namespace nodel;

TEST(LCS, NoMatch) {
    String lhs = "abcd";
    String rhs = "efgh";
    algo::LCS<String> lcs_search;
    String lcs;
    auto len = lcs_search.search(lhs, rhs, &lcs);
    ASSERT_EQ(len, 0UL);
    EXPECT_EQ(lcs, "");
}

TEST(LCS, Basic) {
    String lhs = "pAzBCD";
    String rhs = "qABxxCyD";
    algo::LCS<String> lcs_search;
    String lcs;
    auto len = lcs_search.search(lhs, rhs, &lcs);
    ASSERT_EQ(len, 4UL);
    EXPECT_EQ(lcs, "ABCD");
}

TEST(LCS, EmptyStrings) {
    String lhs = "";
    String rhs = "efgh";
    algo::LCS<String> lcs_search;
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
    algo::LCS<ObjectList> lcs_search;
    ObjectList lcs;
    auto len = lcs_search.search(lhs.as<ObjectList>(), rhs.as<ObjectList>(), &lcs);
    ASSERT_EQ(len, 2UL);
    EXPECT_EQ(Object{lcs}, "['carrot', 'orange']"_json);
}
