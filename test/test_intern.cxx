/// @file
/// @b License: @n Apache License v2.0
/// @copyright Robert Dunnagan
#include <gtest/gtest.h>
#include <fmt/format.h>
#include <nodel/support/intern.hxx>
#include <thread>
#include <cstring>
#include <vector>

using namespace nodel;

TEST(Intern, InternStringLiteral) {
    clear_thread_interns();
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
    std::memcpy(p_non_literal, "tea", 4);
    auto intern4 = intern_string(p_non_literal);
    EXPECT_EQ(intern4.data().data(), intern.data().data());
    delete [] p_non_literal;

    EXPECT_EQ(impl::thread_interns().strings.size(), 1UL);
}

TEST(Intern, InternString) {
    clear_thread_interns();
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

TEST(Intern, ClearThreadInterns) {
    clear_thread_interns();
    intern_string("tea");
    intern_string(std::string(1000, 'x'));
    EXPECT_EQ(impl::thread_interns().strings.size(), 2UL);

    clear_thread_interns();
    EXPECT_EQ(impl::thread_interns().strings.size(), 0UL);

    auto tea = intern_string("tea");
    EXPECT_EQ(tea.data(), "tea");
    EXPECT_EQ(intern_string(std::string("tea")).data().data(), tea.data().data());
    EXPECT_EQ(impl::thread_interns().strings.size(), 1UL);
}

TEST(Intern, UseGlobalInterns) {
    EXPECT_FALSE(using_global_interns());
    EXPECT_FALSE(use_global_interns());
    EXPECT_TRUE(using_global_interns());
    EXPECT_TRUE(use_global_interns(false));
    EXPECT_FALSE(using_global_interns());
}

TEST(Intern, GlobalScopeRestores) {
    EXPECT_FALSE(using_global_interns());
    {
        GlobalInternScope scope;
        EXPECT_TRUE(using_global_interns());
        {
            GlobalInternScope inner;
            EXPECT_TRUE(using_global_interns());
        }
        EXPECT_TRUE(using_global_interns());
    }
    EXPECT_FALSE(using_global_interns());
}

TEST(Intern, GlobalSharedAcrossThreads) {
    GlobalInternScope scope;
    auto tea = intern_string(std::string("tea"));

    intern_t other_tea = tea;
    bool other_uses_global = true;
    std::thread thread{[&] () {
        other_uses_global = using_global_interns();
        GlobalInternScope scope;
        other_tea = intern_string(std::string("tea"));
    }};
    thread.join();

    EXPECT_FALSE(other_uses_global);  // the selection is per-thread
    EXPECT_EQ(tea.data().data(), other_tea.data().data());
    EXPECT_TRUE(tea == other_tea);
}

TEST(Intern, GlobalMixedWithThreadLocal) {
    auto local_tea = intern_string(std::string("tea"));
    GlobalInternScope scope;
    auto global_tea = intern_string(std::string("tea"));
    auto global_coffee = intern_string(std::string("coffee"));
    EXPECT_NE(local_tea.data().data(), global_tea.data().data());
    EXPECT_EQ(local_tea.hash(), global_tea.hash());
    EXPECT_TRUE(local_tea == global_tea);
    EXPECT_TRUE(global_tea == local_tea);
    EXPECT_FALSE(local_tea == global_coffee);
}

TEST(Intern, GlobalConcurrent) {
    clear_global_interns();
    constexpr int n_threads = 8;
    constexpr int n_strings = 1000;
    std::vector<std::vector<const char*>> results(n_threads);
    std::vector<std::thread> threads;
    for (int t = 0; t < n_threads; ++t) {
        threads.emplace_back([&results, t] () {
            GlobalInternScope scope;
            auto& result = results[t];
            for (int i = 0; i < n_strings; ++i) {
                // Each thread interns the strings in a different order.
                int j = (i * 7 + t * 131) % n_strings;
                result.push_back(intern_string(fmt::format("key{}", j)).data().data());
            }
        });
    }
    for (auto& thread : threads)
        thread.join();

    EXPECT_EQ(impl::global_interns().size(), (size_t)n_strings);

    GlobalInternScope scope;
    for (int t = 0; t < n_threads; ++t) {
        for (int i = 0; i < n_strings; ++i) {
            int j = (i * 7 + t * 131) % n_strings;
            EXPECT_EQ(results[t][i], intern_string(fmt::format("key{}", j)).data().data());
        }
    }
}

TEST(Intern, ClearGlobalInterns) {
    GlobalInternScope scope;
    clear_global_interns();
    intern_string("tea");
    intern_string(std::string(1000, 'x'));
    EXPECT_EQ(impl::global_interns().size(), 2UL);

    clear_global_interns();
    EXPECT_EQ(impl::global_interns().size(), 0UL);

    auto tea = intern_string("tea");
    EXPECT_EQ(tea.data(), "tea");
    EXPECT_EQ(intern_string(std::string("tea")).data().data(), tea.data().data());
    EXPECT_EQ(impl::global_interns().size(), 1UL);
}
