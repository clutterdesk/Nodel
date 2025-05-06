/// @file
/// @b License: @n Apache License v2.0
/// @copyright Robert Dunnagan
#include <gtest/gtest.h>
#include <fmt/core.h>

#include <nodel/support/Ref.hxx>

using namespace nodel;

struct Thing
{
    Thing() = default;
    Thing(refcnt_t ref_count) : m_ref_count(ref_count) {}
    refcnt_t m_ref_count;
};

TEST(Ref, RefCountShouldBeOneAfterCreation) {
    Ref<Thing> r_thing = new Thing(3);
    EXPECT_EQ(r_thing.ref_count(), 1);
}

TEST(Ref, CreateAndDelete) {
    auto p_thing = new Thing();
    /* block scope */ {
      Ref<Thing> r_thing = p_thing;
      p_thing->m_ref_count = 2;
    }
    EXPECT_EQ(p_thing->m_ref_count, 1UL);
    delete p_thing;
}

TEST(Ref, CopyRefCountIntegrity) {
    auto p_thing = new Thing();
    Ref<Thing> r_thing = p_thing;
    EXPECT_EQ(p_thing->m_ref_count, 1UL);
    Ref<Thing> r_copy{r_thing};
    EXPECT_EQ(p_thing->m_ref_count, 2UL);
}

TEST(Ref, MoveRefCountIntegrity) {
    auto p_thing = new Thing();
    Ref<Thing> r_thing = p_thing;
    p_thing->m_ref_count = 2;
    Ref<Thing> r_copy{std::move(r_thing)};
    EXPECT_EQ(p_thing->m_ref_count, 2UL);
}

TEST(Ref, CopyAssignRefCountIntegrity) {
    auto p1 = new Thing();
    auto p2 = new Thing();
    /* block scope */ {
        Ref<Thing> r1{p1};
        Ref<Thing> r2{p2};
        r1->m_ref_count = 2;
        r2->m_ref_count = 2;

        r2 = r1;
        EXPECT_EQ(p1->m_ref_count, 3UL);
        EXPECT_EQ(p2->m_ref_count, 1UL);
    }
    EXPECT_EQ(p1->m_ref_count, 1UL);
    EXPECT_EQ(p2->m_ref_count, 1UL);
    delete p1;
    delete p2;
}

TEST(Ref, MoveAssignRefCountIntegrity) {
    auto p1 = new Thing();
    auto p2 = new Thing();
    /* block scope */ {
        Ref<Thing> r1{p1};
        Ref<Thing> r2{p2};
        r1->m_ref_count = 2;
        r2->m_ref_count = 2;

        r2 = std::move(r1);
        EXPECT_EQ(p1->m_ref_count, 2UL);
        EXPECT_EQ(p2->m_ref_count, 1UL);
    }
    EXPECT_EQ(p1->m_ref_count, 1UL);
    EXPECT_EQ(p2->m_ref_count, 1UL);
    delete p1;
    delete p2;
}

TEST(Ref, AssignToEmpty) {
    Ref<Thing> r1;
    Ref<Thing> r2{new Thing()};
    EXPECT_TRUE((bool)r2);
    r1 = r2;
    EXPECT_TRUE((bool)r1);
}
