#include <gtest/gtest.h>
#include <nodel/nodel.h>

#include <sstream>

#include <nodel/impl/Stopwatch.h>

using namespace nodel;
using namespace nodel::json::impl;

TEST(Json, ParseNumberSignedInt) {
  std::stringstream stream{"-37"};
  Parser parser{stream};
  ASSERT_TRUE(parser.parse_number());
  EXPECT_EQ(parser.curr.as<Int>(), -37);
}

TEST(Json, ParseNumberUnsignedInt) {
  UInt value = 0xFFFFFFFFFFFFFFFFULL;
  std::stringstream stream{std::to_string(value)};
  Parser parser{stream};
  ASSERT_TRUE(parser.parse_number());
  EXPECT_EQ(parser.curr.as<UInt>(), value);
}

TEST(Json, ParseNumberRangeError) {
  std::stringstream stream{"1000000000000000000000"};
  Parser parser{stream};
  EXPECT_FALSE(parser.parse_number());
  EXPECT_TRUE(parser.error_message.length() > 0);
}

TEST(Json, ParseNumberFloat) {
  std::stringstream stream{"3.14159"};
  Parser parser{stream};
  EXPECT_TRUE(parser.parse_number());
  EXPECT_EQ(parser.curr.as<Float>(), 3.14159);
}

TEST(Json, ParseNumberDanglingPeriod) {
  std::stringstream stream{"3."};
  Parser parser{stream};
  EXPECT_TRUE(parser.parse_number());
  EXPECT_EQ(parser.curr.as<Float>(), 3.0);
}

TEST(Json, ParseNumberPositiveExponent) {
  std::stringstream stream{"100E+3"};
  Parser parser1{stream};
  EXPECT_TRUE(parser1.parse_number());
  EXPECT_EQ(parser1.curr.as<Float>(), 100000.0);

  std::stringstream stream2{"100E+3"};
  Parser parser2{stream2};
  EXPECT_TRUE(parser2.parse_number());
  EXPECT_EQ(parser2.curr.as<Float>(), 100000.0);
}

TEST(Json, ParseNumberNegativeExponent) {
  std::stringstream stream{"1000E-3"};
  Parser parser1{stream};
  EXPECT_TRUE(parser1.parse_number());
  EXPECT_EQ(parser1.curr.as<Float>(), 1.0);

  std::stringstream stream2{"1000e-3"};
  Parser parser2{stream2};
  EXPECT_TRUE(parser2.parse_number());
  EXPECT_EQ(parser2.curr.as<Float>(), 1.0);
}

TEST(Json, ParseNumberCommaTerminator) {
  std::stringstream stream{"100,"};
  Parser parser1{stream};
  EXPECT_TRUE(parser1.parse_number());
  EXPECT_EQ(parser1.curr.as<Int>(), 100);
}

TEST(Json, ParseNumberMinusSignAlone) {
  std::stringstream stream{"-"};
  Parser parser1{stream};
  EXPECT_FALSE(parser1.parse_number());
}

TEST(Json, ParseNumberMinusSignWithTerminator) {
  std::stringstream stream{"-,"};
  Parser parser1{stream};
  EXPECT_FALSE(parser1.parse_number());
}

TEST(Json, ParseListEmpty) {
  std::stringstream stream{"[]"};
  Parser parser1{stream};
  EXPECT_TRUE(parser1.parse_list());
  EXPECT_EQ(parser1.curr.size(), 0);
}

TEST(Json, ParseListOneInt) {
  std::stringstream stream{"[2]"};
  Parser parser1{stream};
  EXPECT_TRUE(parser1.parse_list());
  Object curr = parser1.curr;
  EXPECT_EQ(curr.size(), 1);
  EXPECT_EQ(curr[0].as<Int>(), 2);
}

TEST(Json, ParseListThreeInts) {
  std::stringstream stream{"[2, 4, 6]"};
  Parser parser1{stream};
  EXPECT_TRUE(parser1.parse_list());
  Object curr = parser1.curr;
  EXPECT_EQ(curr.size(), 3);
  EXPECT_EQ(curr[0].as<Int>(), 2);
  EXPECT_EQ(curr[1].as<Int>(), 4);
  EXPECT_EQ(curr[2].as<Int>(), 6);
}

TEST(Json, ParseExample1) {
    std::stringstream stream{R"({"x": [1], "y": [2]})"};
  Parser parser1{stream};
    EXPECT_TRUE(parser1.parse_object());
    Object curr = parser1.curr;
    EXPECT_TRUE(curr.is_map());
    EXPECT_EQ(curr.size(), 2);
    EXPECT_EQ(curr["x"][0], 1);
    EXPECT_EQ(curr["y"][0], 2);
}

TEST(Json, ParseExampleFile) {
    std::string error;
    Object example = json::parse_file("test/example.json", error);
    EXPECT_EQ(error, "");
    EXPECT_TRUE(example.is_map());
    EXPECT_EQ(example["favorite"], "Assam");
}

TEST(Json, ParseLargeExample1File) {
    std::string error;
    Object example = json::parse_file("test/large_example_1.json", error);
    EXPECT_EQ(error, "");
    EXPECT_TRUE(example.is_list());
}

TEST(Json, ParseLargeExample2File) {
    std::string error;
    Object example = json::parse_file("test/large_example_2.json", error);
    EXPECT_EQ(error, "");
    EXPECT_TRUE(example.is_map());
}

//
// JSON Syntax Errors
//
TEST(Json, ParseListErrantColon) {
    std::stringstream stream{R"(["a", :"b", "c"])"};
  Parser parser{stream};
    EXPECT_FALSE(parser.parse_object());
    EXPECT_NE(parser.error_message, "");
    EXPECT_EQ(parser.error_offset, 6);
}

TEST(Json, ParseListDoubleComma) {
    std::stringstream stream{R"(["a",, "b"])"};
  Parser parser{stream};
    EXPECT_FALSE(parser.parse_object());
    EXPECT_NE(parser.error_message, "");
    EXPECT_EQ(parser.error_offset, 5);
}

TEST(Json, ParseMapDoubleComma) {
    std::stringstream stream{R"({"a": [1],, "b"})"};
  Parser parser{stream};
    EXPECT_FALSE(parser.parse_object());
    EXPECT_NE(parser.error_message, "");
    EXPECT_EQ(parser.error_offset, 10);
}

TEST(Json, ParseErrorBadNumberInList) {
  std::stringstream stream{"[2x]"};
  Parser parser1{stream};
  EXPECT_FALSE(parser1.parse_list());
  EXPECT_EQ(parser1.error_offset, 2);
}
