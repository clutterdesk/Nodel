#include <gtest/gtest.h>
#include <nodel/nodel.h>

using Parser = nodel::Parser;

TEST(Json, ParseNumberSignedInt) {
	Parser parser{"-37"};
	ASSERT_TRUE(parser.parse_number());
	EXPECT_EQ(parser.curr.as_signed_int(), -37);
}

TEST(Json, ParseNumberUnsignedInt) {
	uint64_t value = 0xFFFFFFFFFFFFFFFFULL;
	Parser parser{std::to_string(value)};
	ASSERT_TRUE(parser.parse_number());
	EXPECT_EQ(parser.curr.as_unsigned_int(), value);
}

TEST(Json, ParseNumberRangeError) {
	Parser parser{"1000000000000000000000"};
	EXPECT_FALSE(parser.parse_number());
	EXPECT_TRUE(parser.error_message.length() > 0);
}

TEST(Json, ParseNumberFloat) {
	Parser parser{"3.14159"};
	EXPECT_TRUE(parser.parse_number());
	EXPECT_EQ(parser.curr.as_double(), 3.14159);
}

TEST(Json, ParseNumberDanglingPeriod) {
	Parser parser{"3."};
	EXPECT_TRUE(parser.parse_number());
	EXPECT_EQ(parser.curr.as_double(), 3.0);
}

TEST(Json, ParseNumberPositiveExponent) {
	Parser parser1{"100E+3"};
	EXPECT_TRUE(parser1.parse_number());
	EXPECT_EQ(parser1.curr.as_double(), 100000.0);

	Parser parser2{"100e+3"};
	EXPECT_TRUE(parser2.parse_number());
	EXPECT_EQ(parser2.curr.as_double(), 100000.0);
}

TEST(Json, ParseNumberNegativeExponent) {
	Parser parser1{"1000E-3"};
	EXPECT_TRUE(parser1.parse_number());
	EXPECT_EQ(parser1.curr.as_double(), 1.0);

	Parser parser2{"1000e-3"};
	EXPECT_TRUE(parser2.parse_number());
	EXPECT_EQ(parser2.curr.as_double(), 1.0);
}

TEST(Json, ParseNumberCommaTerminator) {
	Parser parser1{"100,"};
	EXPECT_TRUE(parser1.parse_number());
	EXPECT_EQ(parser1.curr.as_signed_int(), 100);
}

TEST(Json, ParseNumberMinusSignAlone) {
	Parser parser1{"-"};
	EXPECT_FALSE(parser1.parse_number());
}

TEST(Json, ParseNumberMinusSignWithTerminator) {
	Parser parser1{"-,"};
	EXPECT_FALSE(parser1.parse_number());
}

TEST(Json, ParseListEmpty) {
	Parser parser1{"[]"};
	EXPECT_TRUE(parser1.parse_list());
	EXPECT_EQ(parser1.curr.as_list().size(), 0);
}

TEST(Json, ParseListOneInt) {
	Parser parser1{"[2]"};
	EXPECT_TRUE(parser1.parse_list());
	auto& list = parser1.curr.as_list();
	EXPECT_EQ(list.size(), 1);
	EXPECT_EQ(list[0].as_signed_int(), 2);
}

TEST(Json, ParseListThreeInts) {
	Parser parser1{"[2, 4, 6]"};
	EXPECT_TRUE(parser1.parse_list());
	auto& list = parser1.curr.as_list();
	EXPECT_EQ(list.size(), 3);
	EXPECT_EQ(list[0].as_signed_int(), 2);
	EXPECT_EQ(list[1].as_signed_int(), 4);
	EXPECT_EQ(list[2].as_signed_int(), 6);
}

