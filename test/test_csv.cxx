/// @file
/// @b License: @n Apache License v2.0
/// @copyright Robert Dunnagan
#include <gtest/gtest.h>

#include <fmt/format.h>
#include <nodel/parser/csv.hxx>
#include <nodel/serializer/CsvSerializer.hxx>

using namespace nodel;
using namespace nodel::csv;
using namespace nodel::csv::impl;

TEST(CsvParser, Unquoted) {
  std::stringstream stream{R"(
      a, bbb, cc
      dd, e, f
      g,hh,iii)"};

  Parser parser{stream};
  Object obj = parser.parse();
  EXPECT_EQ(obj.to_str(), R"([["a", "bbb", "cc"], ["dd", "e", "f"], ["g", "hh", "iii"]])");
}

TEST(CsvParser, SingleQuoted) {
  std::stringstream stream{R"(
      'a', 'bbb', 'cc'
      'dd', 'e', 'f'
      'g','hh','iii'
  )"};

  Parser parser{stream};
  Object obj = parser.parse();
  EXPECT_EQ(obj.to_str(), R"([["a", "bbb", "cc"], ["dd", "e", "f"], ["g", "hh", "iii"]])");
}

TEST(CsvParser, DoubleQuoted) {
  std::stringstream stream{R"(
      "a", "bbb", "cc"
      "dd", "e", "f"
      "g","hh","iii"
  )"};

  Parser parser{stream};
  Object obj = parser.parse();
  EXPECT_EQ(obj.to_str(), R"([["a", "bbb", "cc"], ["dd", "e", "f"], ["g", "hh", "iii"]])");
}

TEST(CsvParser, MixedQuoted) {
  std::stringstream stream{R"(
      a, "bbb", 'cc'
      'dd', e,f
      'g',"hh",iii
  )"};

  Parser parser{stream};
  Object obj = parser.parse();
  EXPECT_EQ(obj.to_str(), R"([["a", "bbb", "cc"], ["dd", "e", "f"], ["g", "hh", "iii"]])");
}

TEST(CsvParser, UnquotedWithSpaces) {
  std::stringstream stream{R"(
      Title, Author
      Moby Dick, Herman Melville
      The Name of the Rose, Umberto Eco
      Middlemarch, George Elliot
  )"};

  Parser parser{stream};
  Object obj = parser.parse();
  EXPECT_EQ(obj.size(), 4UL);
  for (size_t i=0; i<obj.size(); i++)
      EXPECT_EQ(obj.get(i).size(), 2UL);
  EXPECT_EQ(obj.get(1).get(0), "Moby Dick");
  EXPECT_EQ(obj.get(1).get(1), "Herman Melville");
  EXPECT_EQ(obj.get(2).get(0), "The Name of the Rose");
  EXPECT_EQ(obj.get(2).get(1), "Umberto Eco");
  EXPECT_EQ(obj.get(3).get(0), "Middlemarch");
  EXPECT_EQ(obj.get(3).get(1), "George Elliot");
}

TEST(CsvParser, EmptyCell) {
    std::stringstream stream{R"(
        1,2,3
        3,,4
        ,,
        ,,5
    )"};

    Parser parser{stream};
    Object obj = parser.parse();
    ASSERT_EQ(obj.size(), 4UL);
    EXPECT_EQ(obj.get(0).size(), 3UL);
    EXPECT_EQ(obj.get(1).size(), 3UL);
    EXPECT_EQ(obj.get(2).size(), 3UL);
    EXPECT_EQ(obj.get(3).size(), 3UL);
    EXPECT_EQ(obj.get(0).get(0), 1);
    EXPECT_EQ(obj.get(3).get(2), 5);
}

TEST(CsvParser, CellWithHyphen) {
    std::stringstream stream{R"(
        2025-03-19, '1 boiled egg, oatmeal'
    )"};

    Parser parser{stream};
    Object obj = parser.parse();
    ASSERT_EQ(obj.size(), 1UL);
    EXPECT_EQ(obj.get(0).size(), 2UL);
    EXPECT_EQ(obj.get(0).get(0), "2025-03-19");
}

TEST(CsvParser, UnquotedWithInteger) {
    std::stringstream stream{R"(2469216E2BPD7VETL AMAZON MKTPL*A82M02I43   Amzn.com/billWA           ,,,,"-$00.00")"};

    Parser parser{stream};
    Object obj = parser.parse();
    ASSERT_EQ(obj.size(), 1UL);
    EXPECT_EQ(obj.get(0).size(), 5UL);
    EXPECT_EQ(obj.get(0).get(0), "2469216E2BPD7VETL AMAZON MKTPL*A82M02I43   Amzn.com/billWA");
    EXPECT_EQ(obj.get(0).get(4), "-$00.00");
}

TEST(CsvParser, UnterminatedQuote) {
    std::stringstream stream{R"(a,"abc)"};

    Parser parser{stream};
    Object obj = parser.parse();
    EXPECT_FALSE(obj.is_valid());
    EXPECT_NE(parser.error().find("Unterminated quoted string"), std::string::npos);
}

TEST(CsvParser, UnterminatedDoubleQuoteAfterNewline) {
    std::stringstream stream{"\"abc\nd,e\n"};

    Parser parser{stream};
    Object obj = parser.parse();
    EXPECT_FALSE(obj.is_valid());
    EXPECT_NE(parser.error().find("Unterminated quoted string"), std::string::npos);
}

TEST(CsvParser, UnterminatedQuoteEndsWithDoubledQuote) {
    std::stringstream stream{R"("ab"")"};

    Parser parser{stream};
    Object obj = parser.parse();
    EXPECT_FALSE(obj.is_valid());
    EXPECT_NE(parser.error().find("Unterminated quoted string"), std::string::npos);
}

TEST(CsvParser, QuotedWithDoubledQuotes) {
    std::stringstream stream{R"("a""b", 'c''d', """", '''', "", '', "x""""y")"};

    Parser parser{stream};
    Object obj = parser.parse();
    EXPECT_EQ(obj.to_json(), R"([["a\"b", "c'd", "\"", "'", "", "", "x\"\"y"]])");
}

TEST(CsvParser, BackslashDoesNotEscapeQuote) {
    std::stringstream stream{R"("a\"b")"};

    Parser parser{stream};
    Object obj = parser.parse();
    EXPECT_FALSE(obj.is_valid());
    EXPECT_NE(parser.error().find("Expected comma or new-line"), std::string::npos);
}

TEST(CsvParser, QuotedAtEndOfInput) {
    std::stringstream stream{R"(a,"bc")"};

    Parser parser{stream};
    Object obj = parser.parse();
    ASSERT_EQ(obj.size(), 1UL);
    EXPECT_EQ(obj.get(0).to_json(), R"(["a", "bc"])");
}

TEST(CsvParser, DoubleQuotedSpansLines) {
    std::stringstream stream{"\"a\nb\",c\n"};

    Parser parser{stream};
    Object obj = parser.parse();
    ASSERT_EQ(obj.size(), 1UL);
    EXPECT_EQ(obj.get(0).get(0), "a\nb");
    EXPECT_EQ(obj.get(0).get(1), "c");
}

TEST(CsvParser, LeadingApostropheNotClosedOnLine) {
    std::stringstream stream{"'90s,x\ny,z\n"};

    Parser parser{stream};
    Object obj = parser.parse();
    EXPECT_EQ(obj.to_json(), R"([["'90s", "x"], ["y", "z"]])");
}

TEST(CsvParser, LeadingApostropheAtEndOfInput) {
    std::stringstream stream{"a,'til"};

    Parser parser{stream};
    Object obj = parser.parse();
    EXPECT_EQ(obj.to_json(), R"([["a", "'til"]])");
}

TEST(CsvParser, LeadingApostropheClosedMidField) {
    std::stringstream stream{"'90s music, '80s rock\n"};

    Parser parser{stream};
    Object obj = parser.parse();
    EXPECT_EQ(obj.to_json(), R"([["'90s music", "'80s rock"]])");
}

TEST(CsvParser, LeadingApostropheWithQuotedFieldLater) {
    std::stringstream stream{"'a, \"b,c\", 'd'\n"};

    Parser parser{stream};
    Object obj = parser.parse();
    EXPECT_EQ(obj.to_json(), R"([["'a", "b,c", "d"]])");
}

TEST(CsvParser, LeadingApostropheTextAfterClosingQuote) {
    std::stringstream stream{"'a' b,c\n'it''s' ,d\r\n"};

    Parser parser{stream};
    Object obj = parser.parse();
    EXPECT_EQ(obj.to_json(), R"([["'a' b", "c"], ["it's", "d"]])");
}

TEST(CsvParser, QuotedWithLiteralBackslash) {
    std::stringstream stream{R"("C:\Users\x", 'D:\tmp\new', "a\nb", "\\server\share", "a\")"};

    Parser parser{stream};
    Object obj = parser.parse();
    ASSERT_EQ(obj.size(), 1UL);
    EXPECT_EQ(obj.get(0).get(0), R"(C:\Users\x)");
    EXPECT_EQ(obj.get(0).get(1), R"(D:\tmp\new)");
    EXPECT_EQ(obj.get(0).get(2), R"(a\nb)");
    EXPECT_EQ(obj.get(0).get(3), R"(\\server\share)");
    EXPECT_EQ(obj.get(0).get(4), R"(a\)");
}

TEST(CsvParser, QuotedWithOtherQuoteEscaped) {
    std::stringstream stream{R"("a\'b", 'c\"d')"};

    Parser parser{stream};
    Object obj = parser.parse();
    ASSERT_EQ(obj.size(), 1UL);
    EXPECT_EQ(obj.get(0).get(0), R"(a\'b)");
    EXPECT_EQ(obj.get(0).get(1), R"(c\"d)");
}

TEST(CsvParser, SerializerRoundTrip) {
    Object table = ObjectList{};
    Object row = ObjectList{};
    row.set(0, R"(C:\Users\x)");
    row.set(1, R"(\\server\share)");
    row.set(2, R"(say "hi"\)");
    row.set(3, "a,b\nc");
    row.set(4, "it's");
    row.set(5, "123");
    row.set(6, 7);
    table.set(0, row);

    CsvSerializer serializer;
    std::stringstream stream;
    serializer.write(stream, table, nil);
    EXPECT_EQ(stream.str(), "\"C:\\Users\\x\",\"\\\\server\\share\",\"say \"\"hi\"\"\\\",\"a,b\nc\",\"it's\",\"123\",7\n");
    Object obj = serializer.read(stream, 0);
    ASSERT_EQ(obj.size(), 1UL);
    ASSERT_EQ(obj.get(0).size(), 7UL);
    EXPECT_EQ(obj.to_json(), table.to_json());
}

TEST(CsvParser, NonAsciiText) {
    std::stringstream stream{"caf\xC3\xA9 , na\xC3\xAFve,\xC3\xA9\n'\xC5\x93uf' ,\"\xE6\x97\xA5\xE6\x9C\xAC\"\n"};

    Parser parser{stream};
    Object obj = parser.parse();
    ASSERT_EQ(obj.size(), 2UL);
    EXPECT_EQ(obj.get(0).get(0), "caf\xC3\xA9");
    EXPECT_EQ(obj.get(0).get(1), "na\xC3\xAFve");
    EXPECT_EQ(obj.get(0).get(2), "\xC3\xA9");
    EXPECT_EQ(obj.get(1).get(0), "\xC5\x93uf");
    EXPECT_EQ(obj.get(1).get(1), "\xE6\x97\xA5\xE6\x9C\xAC");
}

TEST(Trim, EmptyAndAllWhitespace) {
    EXPECT_EQ(ltrim(""), "");
    EXPECT_EQ(rtrim(""), "");
    EXPECT_EQ(trim(""), "");
    EXPECT_EQ(ltrim(" \t\n"), "");
    EXPECT_EQ(rtrim(" \t\n"), "");
    EXPECT_EQ(trim(" \t\n"), "");
}

TEST(Trim, LeadingAndTrailing) {
    EXPECT_EQ(ltrim(" a b "), "a b ");
    EXPECT_EQ(rtrim(" a b "), " a b");
    EXPECT_EQ(trim(" a b "), "a b");
    EXPECT_EQ(trim("\t\xC3\xA9\n"), "\xC3\xA9");
}
