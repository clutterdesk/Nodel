/// @file
/// @b License: @n Apache License v2.0
/// @copyright Robert Dunnagan
#include <gtest/gtest.h>
#include <fmt/core.h>
#include <nodel/support/intern.hxx>
#include <thread>

using namespace nodel;

TEST(Intern, InternStringLiteral) {
    impl::thread_interns().strings.clear();
    EXPECT_EQ(impl::thread_interns().strings.size(), 0UL);

    const char* literal = "tea";
    auto intern = intern_string_literal(literal);
    EXPECT_EQ(intern.data(), "tea");

    auto intern2 = intern_string_literal(literal);
    EXPECT_EQ(intern2.data().data(), intern.data().data());

    char non_literal[4] = {'t', 'e', 'a', 0};
    auto intern3 = intern_string(non_literal);
    EXPECT_EQ(intern3.data().data(), intern.data().data());

    char* p_non_literal = new char[4];
    strcpy(p_non_literal, "tea");
    auto intern4 = intern_string(p_non_literal);
    EXPECT_EQ(intern4.data().data(), intern.data().data());
    delete [] p_non_literal;

    EXPECT_EQ(impl::thread_interns().strings.size(), 1UL);
}

TEST(Intern, InternString) {
    impl::thread_interns().strings.clear();
    EXPECT_EQ(impl::thread_interns().strings.size(), 0UL);

    auto intern = intern_string_literal("tea");
    EXPECT_EQ(intern.data(), "tea");

    auto intern2 = intern_string(std::string("tea"));
    EXPECT_EQ(intern2.data().data(), intern.data().data());

    auto p_str = new std::string("tea");
    auto intern3 = intern_string(*p_str);
    EXPECT_EQ(intern3.data().data(), intern.data().data());
    delete p_str;

    EXPECT_EQ(impl::thread_interns().strings.size(), 1UL);
}

TEST(Intern, AcrossThreads) {
    auto other_tea = intern_string("tea");
    auto other_coffee = intern_string("coffee");
    std::thread thread{[&] () {
        other_tea = intern_string(std::string("tea"));
        other_coffee = intern_string(std::string("coffee"));
    }};
    thread.join();

    auto tea = intern_string("tea");
    EXPECT_NE(tea.data().data(), other_tea.data().data());
    EXPECT_EQ(tea.hash(), other_tea.hash());
    EXPECT_TRUE(tea == other_tea);
    EXPECT_FALSE(tea == other_coffee);
}
