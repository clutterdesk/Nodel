/// @file
/// @b License: @n Apache License v2.0
/// @copyright Robert Dunnagan
#include <gtest/gtest.h>

#include <fmt/format.h>
#include <nodel/parser/csv.hxx>

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
