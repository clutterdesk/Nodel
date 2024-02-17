#include <gtest/gtest.h>
#include <nodel/nodel.h>

#include <sstream>

using namespace nodel;
using namespace nodel::json::impl;

TEST(Json, ParseNumberSignedInt) {
  Parser parser{std::stringstream{"-37"}};
  ASSERT_TRUE(parser.parse_number());
  EXPECT_EQ(parser.curr.as<Int>(), -37);
}

TEST(Json, ParseNumberUnsignedInt) {
  UInt value = 0xFFFFFFFFFFFFFFFFULL;
  Parser parser{std::istringstream{std::to_string(value)}};
  ASSERT_TRUE(parser.parse_number());
  EXPECT_EQ(parser.curr.as<UInt>(), value);
}

TEST(Json, ParseNumberRangeError) {
  Parser parser{std::stringstream{"1000000000000000000000"}};
  EXPECT_FALSE(parser.parse_number());
  EXPECT_TRUE(parser.error_message.length() > 0);
}

TEST(Json, ParseNumberFloat) {
  Parser parser{std::stringstream{"3.14159"}};
  EXPECT_TRUE(parser.parse_number());
  EXPECT_EQ(parser.curr.as<Float>(), 3.14159);
}

TEST(Json, ParseNumberDanglingPeriod) {
  Parser parser{std::stringstream{"3."}};
  EXPECT_TRUE(parser.parse_number());
  EXPECT_EQ(parser.curr.as<Float>(), 3.0);
}

TEST(Json, ParseNumberPositiveExponent) {
  Parser parser1{std::stringstream{"100E+3"}};
  EXPECT_TRUE(parser1.parse_number());
  EXPECT_EQ(parser1.curr.as<Float>(), 100000.0);

  Parser parser2{std::stringstream{"100e+3"}};
  EXPECT_TRUE(parser2.parse_number());
  EXPECT_EQ(parser2.curr.as<Float>(), 100000.0);
}

TEST(Json, ParseNumberNegativeExponent) {
  Parser parser1{std::stringstream{"1000E-3"}};
  EXPECT_TRUE(parser1.parse_number());
  EXPECT_EQ(parser1.curr.as<Float>(), 1.0);

  Parser parser2{std::stringstream{"1000e-3"}};
  EXPECT_TRUE(parser2.parse_number());
  EXPECT_EQ(parser2.curr.as<Float>(), 1.0);
}

TEST(Json, ParseNumberCommaTerminator) {
  Parser parser1{std::stringstream{"100,"}};
  EXPECT_TRUE(parser1.parse_number());
  EXPECT_EQ(parser1.curr.as<Int>(), 100);
}

TEST(Json, ParseNumberMinusSignAlone) {
  Parser parser1{std::stringstream{"-"}};
  EXPECT_FALSE(parser1.parse_number());
}

TEST(Json, ParseNumberMinusSignWithTerminator) {
  Parser parser1{std::stringstream{"-,"}};
  EXPECT_FALSE(parser1.parse_number());
}

TEST(Json, ParseListEmpty) {
  Parser parser1{std::stringstream{"[]"}};
  EXPECT_TRUE(parser1.parse_list());
  EXPECT_EQ(parser1.curr.size(), 0);
}

TEST(Json, ParseListOneInt) {
  Parser parser1{std::stringstream{"[2]"}};
  EXPECT_TRUE(parser1.parse_list());
  Object curr = parser1.curr;
  EXPECT_EQ(curr.size(), 1);
  EXPECT_EQ(curr[0].as<Int>(), 2);
}

TEST(Json, ParseListThreeInts) {
  Parser parser1{std::stringstream{"[2, 4, 6]"}};
  EXPECT_TRUE(parser1.parse_list());
  Object curr = parser1.curr;
  EXPECT_EQ(curr.size(), 3);
  EXPECT_EQ(curr[0].as<Int>(), 2);
  EXPECT_EQ(curr[1].as<Int>(), 4);
  EXPECT_EQ(curr[2].as<Int>(), 6);
}

//
// Error Tests
//

TEST(Json, ParseErrorBadNumberInList) {
  Parser parser1{std::stringstream{"[2x]"}};
  EXPECT_FALSE(parser1.parse_list());
  EXPECT_EQ(parser1.error_offset, 3);
}

