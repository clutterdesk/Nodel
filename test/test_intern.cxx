/// @file
/// @b License: @n Apache License v2.0
/// @copyright Robert Dunnagan
#include <gtest/gtest.h>
#include <fmt/core.h>
#include <nodel/support/intern.hxx>

using namespace nodel;

TEST(Intern, InternStringLiteral) {
    thread_interns.clear();
    EXPECT_EQ(thread_interns.size(), 0UL);

    const char* literal = "tea";
    auto intern = intern_string_literal(literal);
    EXPECT_EQ(intern.data().data(), literal);

    literal = "tea";
    auto intern2 = intern_string_literal(literal);
    EXPECT_EQ(intern2.data().data(), literal);

    char non_literal[4] = {'t', 'e', 'a', 0};
    auto intern3 = intern_string(non_literal);
    EXPECT_EQ(intern3.data().data(), literal);

    char* p_non_literal = new char[4];
    strcpy(p_non_literal, "tea");
    auto intern4 = intern_string(p_non_literal);
    EXPECT_EQ(intern4.data().data(), literal);
    delete [] p_non_literal;

    EXPECT_EQ(thread_interns.size(), 1UL);
}

TEST(Intern, InternString) {
    thread_interns.clear();
    EXPECT_EQ(thread_interns.size(), 0UL);

    const char* literal = "tea";
    auto intern = intern_string_literal(literal);
    EXPECT_EQ(intern.data().data(), literal);

    auto intern2 = intern_string(std::string("tea"));
    EXPECT_EQ(intern2.data().data(), literal);

    auto p_str = new std::string("tea");
    auto intern3 = intern_string(*p_str);
    EXPECT_EQ(intern3.data().data(), literal);
    delete p_str;

    EXPECT_EQ(thread_interns.size(), 1UL);
}
