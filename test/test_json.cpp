#include <gtest/gtest.h>
#include <sstream>

#include <nodel/format/json.h>
#include <nodel/support/Stopwatch.h>

using namespace nodel;
using namespace nodel::json;
using namespace nodel::json::impl;

TEST(Json, ParseNull) {
    std::stringstream stream{"none"};
    Parser parser{nodel::impl::StreamAdapter{stream}};
    ASSERT_TRUE(parser.parse_object());
    EXPECT_TRUE(parser.m_curr == none);
}

TEST(Json, ParseTypeNull) {
    std::stringstream stream{"none"};
    Parser parser{nodel::impl::StreamAdapter{stream}};
    EXPECT_EQ(parser.parse_type(), Object::NONE);
}

TEST(Json, ParseTypeBoolFalse) {
    std::stringstream stream{"false"};
    Parser parser{nodel::impl::StreamAdapter{stream}};
    EXPECT_EQ(parser.parse_type(), Object::BOOL);
}

TEST(Json, ParseBoolTrue) {
    std::stringstream stream{"true"};
    Parser parser{nodel::impl::StreamAdapter{stream}};
    ASSERT_TRUE(parser.parse_object());
    EXPECT_TRUE(parser.m_curr.is_type<bool>());
    EXPECT_EQ(parser.m_curr, true);
}

TEST(Json, ParseTypeBoolTrue) {
    std::stringstream stream{"true"};
    Parser parser{nodel::impl::StreamAdapter{stream}};
    EXPECT_EQ(parser.parse_type(), Object::BOOL);
}

TEST(Json, ParseNumberSignedInt) {
  std::stringstream stream{"-37"};
  Parser parser{nodel::impl::StreamAdapter{stream}};
  ASSERT_TRUE(parser.parse_number());
  EXPECT_EQ(parser.m_curr.as<Int>(), -37);
}

TEST(Json, ParseNumberUnsignedInt) {
  UInt value = 0xFFFFFFFFFFFFFFFFULL;
  std::stringstream stream{std::to_string(value)};
  Parser parser{nodel::impl::StreamAdapter{stream}};
  ASSERT_TRUE(parser.parse_number());
  EXPECT_EQ(parser.m_curr.as<UInt>(), value);
}

TEST(Json, ParseNumberRangeError) {
  std::stringstream stream{"1000000000000000000000"};
  Parser parser{nodel::impl::StreamAdapter{stream}};
  EXPECT_FALSE(parser.parse_number());
  EXPECT_TRUE(parser.m_error_message.length() > 0);
}

TEST(Json, ParseNumberFloat) {
  std::stringstream stream{"3.14159"};
  Parser parser{nodel::impl::StreamAdapter{stream}};
  EXPECT_TRUE(parser.parse_number());
  EXPECT_EQ(parser.m_curr.as<Float>(), 3.14159);
}

TEST(Json, ParseNumberFloatLeadingDecimal) {
  std::stringstream stream{".1"};
  Parser parser{nodel::impl::StreamAdapter{stream}};
  EXPECT_TRUE(parser.parse_number());
  EXPECT_EQ(parser.m_curr.as<Float>(), .1);
}

TEST(Json, ParseTypeFloatLeadingDecimal) {
    std::stringstream stream{".1"};
    Parser parser{nodel::impl::StreamAdapter{stream}};
    EXPECT_EQ(parser.parse_type(), Object::FLOAT);
}

TEST(Json, ParseNumberTrailingDecimal) {
  std::stringstream stream{"3."};
  Parser parser{nodel::impl::StreamAdapter{stream}};
  EXPECT_TRUE(parser.parse_number());
  EXPECT_EQ(parser.m_curr.as<Float>(), 3.0);
}

TEST(Json, ParseNumberPositiveExponent) {
  std::stringstream stream{"100E+3"};
  Parser parser1{nodel::impl::StreamAdapter{stream}};
  EXPECT_TRUE(parser1.parse_number());
  EXPECT_EQ(parser1.m_curr.as<Float>(), 100000.0);

  std::stringstream stream2{"100E+3"};
  Parser parser2{nodel::impl::StreamAdapter{stream2}};
  EXPECT_TRUE(parser2.parse_number());
  EXPECT_EQ(parser2.m_curr.as<Float>(), 100000.0);
}

TEST(Json, ParseNumberNegativeExponent) {
  std::stringstream stream{"1000E-3"};
  Parser parser1{nodel::impl::StreamAdapter{stream}};
  EXPECT_TRUE(parser1.parse_number());
  EXPECT_EQ(parser1.m_curr.as<Float>(), 1.0);

  std::stringstream stream2{"1000e-3"};
  Parser parser2{nodel::impl::StreamAdapter{stream2}};
  EXPECT_TRUE(parser2.parse_number());
  EXPECT_EQ(parser2.m_curr.as<Float>(), 1.0);
}

TEST(Json, ParseNumberCommaTerminator) {
  std::stringstream stream{"100,"};
  Parser parser1{nodel::impl::StreamAdapter{stream}};
  EXPECT_TRUE(parser1.parse_number());
  EXPECT_EQ(parser1.m_curr.as<Int>(), 100);
}

TEST(Json, ParseNumberMinusSignAlone) {
  std::stringstream stream{"-"};
  Parser parser1{nodel::impl::StreamAdapter{stream}};
  EXPECT_FALSE(parser1.parse_number());
}

TEST(Json, ParseNumberMinusSignWithTerminator) {
  std::stringstream stream{"-,"};
  Parser parser1{nodel::impl::StreamAdapter{stream}};
  EXPECT_FALSE(parser1.parse_number());
}

TEST(Json, ParseSingleQuotedString) {
    std::stringstream stream{"'tea'"};
    Parser parser{nodel::impl::StreamAdapter{stream}};
    EXPECT_TRUE(parser.parse_string());
    EXPECT_EQ(parser.m_curr.as<String>(), "tea");
}

TEST(Json, ParseUnterminatedString) {
    std::string err;
    EXPECT_TRUE(parse("'tea", err) == none);
    EXPECT_TRUE(err.size());
}

TEST(Json, ParseTypeStringSingleQuote) {
    std::stringstream stream{"'tea'"};
    Parser parser{nodel::impl::StreamAdapter{stream}};
    EXPECT_EQ(parser.parse_type(), Object::STR);
}

TEST(Json, ParseDoubleQuotedString) {
    std::stringstream stream{"\"tea\""};
    Parser parser{nodel::impl::StreamAdapter{stream}};
    EXPECT_TRUE(parser.parse_string());
    EXPECT_EQ(parser.m_curr.as<String>(), "tea");
}

TEST(Json, ParseTypeStringDoubleQuote) {
    std::stringstream stream{"\"tea\""};
    Parser parser{nodel::impl::StreamAdapter{stream}};
    EXPECT_EQ(parser.parse_type(), Object::STR);
}

TEST(Json, ParseListEmpty) {
  std::stringstream stream{"[]"};
  Parser parser1{nodel::impl::StreamAdapter{stream}};
  EXPECT_TRUE(parser1.parse_list());
  EXPECT_EQ(parser1.m_curr.size(), 0);
}

TEST(Json, ParseListOneInt) {
  std::stringstream stream{"[2]"};
  Parser parser1{nodel::impl::StreamAdapter{stream}};
  EXPECT_TRUE(parser1.parse_list());
  Object curr = parser1.m_curr;
  EXPECT_EQ(curr.size(), 1);
  EXPECT_EQ(curr.get(0).as<Int>(), 2);
}

TEST(Json, ParseListThreeInts) {
  std::stringstream stream{"[2, 4, 6]"};
  Parser parser1{nodel::impl::StreamAdapter{stream}};
  EXPECT_TRUE(parser1.parse_list());
  Object curr = parser1.m_curr;
  EXPECT_EQ(curr.size(), 3);
  EXPECT_EQ(curr.get(0).as<Int>(), 2);
  EXPECT_EQ(curr.get(1).as<Int>(), 4);
  EXPECT_EQ(curr.get(2).as<Int>(), 6);
}

TEST(Json, ParseExample1) {
    std::stringstream stream{R"({"x": [1], "y": [2]})"};
  Parser parser1{nodel::impl::StreamAdapter{stream}};
    EXPECT_TRUE(parser1.parse_object());
    Object curr = parser1.m_curr;
    EXPECT_TRUE(curr.is_type<OrderedMap>());
    EXPECT_EQ(curr.size(), 2);
    EXPECT_EQ(curr.get("x"_key).get(0), 1);
    EXPECT_EQ(curr.get("y"_key).get(0), 2);
}

TEST(Json, ParseExampleFile) {
    std::string error;
    Object example = json::parse_file("test_data/example.json", error);
    EXPECT_EQ(error, "");
    EXPECT_TRUE(example.is_type<OrderedMap>());
    EXPECT_EQ(example.get("favorite"_key), "Assam");
}

TEST(Json, ParseLargeExample1File) {
    std::string error;
    Object example = json::parse_file("test_data/large_example_1.json", error);
    EXPECT_EQ(error, "");
    EXPECT_TRUE(example.is_type<List>());
}

TEST(Json, ParseLargeExample2File) {
    std::string error;
    Object example = json::parse_file("test_data/large_example_2.json", error);
    EXPECT_EQ(error, "");
    EXPECT_TRUE(example.is_type<OrderedMap>());
}

//
// JSON Syntax Errors
//
TEST(Json, ParseListErrantColon) {
    std::stringstream stream{R"(["a", :"b", "c"])"};
  Parser parser{nodel::impl::StreamAdapter{stream}};
    EXPECT_FALSE(parser.parse_object());
    EXPECT_NE(parser.m_error_message, "");
    EXPECT_EQ(parser.m_error_offset, 6);
}

TEST(Json, ParseListDoubleComma) {
    std::stringstream stream{R"(["a",, "b"])"};
  Parser parser{nodel::impl::StreamAdapter{stream}};
    EXPECT_FALSE(parser.parse_object());
    EXPECT_NE(parser.m_error_message, "");
    EXPECT_EQ(parser.m_error_offset, 5);
}

TEST(Json, ParseMapDoubleComma) {
    std::stringstream stream{R"({"a": [1],, "b"})"};
  Parser parser{nodel::impl::StreamAdapter{stream}};
    EXPECT_FALSE(parser.parse_object());
    EXPECT_NE(parser.m_error_message, "");
    EXPECT_EQ(parser.m_error_offset, 10);
}

TEST(Json, ParseErrorBadNumberInList) {
  std::stringstream stream{"[2x]"};
  Parser parser1{nodel::impl::StreamAdapter{stream}};
  EXPECT_FALSE(parser1.parse_list());
  EXPECT_EQ(parser1.m_error_offset, 2);
}
