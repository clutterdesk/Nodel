#include <gtest/gtest.h>
#include <fmt/core.h>
#include <unordered_set>

#include <nodel/core/Object.h>
#include <nodel/core/Query.h>
#include <nodel/serialization/json.h>

using namespace nodel;
using Axis = Step::Axis;

TEST(Query, ChildStep) {
    Query query;
    query.add_step({Axis::CHILD, "x"_key});

    Object obj = json::parse("{'x': 'tea'}");
    auto it = query.eval(obj);

    Object it_obj = it.next();
    EXPECT_TRUE(it_obj != null);
    EXPECT_EQ(it_obj, "tea");

    it_obj = it.next();
    EXPECT_TRUE(it_obj == null);
}

TEST(Query, ChildAnyStep) {
    Query query;
    query.add_step({Axis::CHILD, null});

    Object obj = json::parse("{'x': 'tea'}");
    auto it = query.eval(obj);

    Object it_obj = it.next();
    EXPECT_TRUE(it_obj != null);
    EXPECT_EQ(it_obj, "tea");

    it_obj = it.next();
    EXPECT_TRUE(it_obj == null);
}

TEST(Query, ParentStep) {
    Query query;
    query.add_step({Axis::PARENT, "x"_key});

    Object obj = json::parse("{'x': {'y': 'tea'}}");
    auto it = query.eval(obj.get("x"_key).get("y"_key));

    Object it_obj = it.next();
    EXPECT_TRUE(it_obj != null);
    EXPECT_TRUE(it_obj.is(obj.get("x"_key)));

    it_obj = it.next();
    EXPECT_TRUE(it_obj == null);
}

TEST(Query, ParentAnyStep) {
    Query query;
    query.add_step({Axis::PARENT, null});

    Object obj = json::parse("{'x': 'tea'}");
    auto it = query.eval(obj.get("x"_key));

    Object it_obj = it.next();
    EXPECT_TRUE(it_obj != null);
    EXPECT_TRUE(it_obj.is(obj));

    it_obj = it.next();
    EXPECT_TRUE(it_obj == null);
}

TEST(Query, AncestorAnyStep) {
    Query query;
    query.add_step({Axis::ANCESTOR, null});

    Object obj = json::parse("{'x': {'y': 'tea'}}");
    Object x = obj.get("x"_key);
    Object y = x.get("y"_key);

    auto it = query.eval(y);

    EXPECT_TRUE(it.next().is(x));
    EXPECT_TRUE(it.next().is(obj));
    EXPECT_TRUE(it.next() == null);
}

TEST(Query, AncestorStep) {
    Query query;
    query.add_step({Axis::ANCESTOR, "y"_key});

    Object obj = json::parse("{'x': {'y': {'y': 'tea'}}}");
    Object x = obj.get("x"_key);
    Object y = x.get("y"_key);
    Object yy = y.get("y"_key);

    auto it = query.eval(yy);

    EXPECT_TRUE(it.next().is(y));
    EXPECT_TRUE(it.next() == null);
}

TEST(Query, SubtreeAnyStep) {
    Query query;
    query.add_step({Axis::SUBTREE, null});

    Object root = json::parse("{'x': [{'u': 'x0u', 'v': 'x0v'}, {'z': 'x1z'}], 'y': {'u': [['xyu00', 'xyu01'], 'xyu1']}}}");

    std::unordered_set<Oid> expect;
    for (auto obj : root.iter_tree()) { expect.insert(obj.id()); }

    auto it = query.eval(root);
    auto count = 0;
    for (auto it_obj = it.next(); it_obj != null; it_obj = it.next(), ++count) {
        EXPECT_TRUE(expect.contains(it_obj.id()));
    }

    EXPECT_EQ(count, expect.size());
}


