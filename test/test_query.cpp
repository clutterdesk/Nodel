#include <gtest/gtest.h>
#include <fmt/core.h>
#include <unordered_set>

#include <nodel/core/Object.h>
#include <nodel/core/Query.h>
#include <nodel/core/Query.h>
#include <nodel/core/bind.h>
#include <nodel/filesystem.h>
#include <nodel/fmt_support.h>

using namespace nodel;


TEST(Query, ChildStep) {
    Query query;
    query.add_steps({Axis::CHILD, "x"_key});

    Object obj = json::parse("{'x': 'tea'}");
    auto it = query.iter_eval(obj);

    Object it_obj = it.next();
    EXPECT_TRUE(it_obj != nil);
    EXPECT_EQ(it_obj, "tea");

    it_obj = it.next();
    EXPECT_TRUE(it_obj == nil);
}

TEST(Query, ChildAnyStep) {
    Query query;
    query.add_steps({Axis::CHILD, nil});

    Object obj = json::parse("{'x': 'tea'}");
    auto it = query.iter_eval(obj);

    Object it_obj = it.next();
    EXPECT_TRUE(it_obj != nil);
    EXPECT_EQ(it_obj, "tea");

    it_obj = it.next();
    EXPECT_TRUE(it_obj == nil);
}

TEST(Query, ParentStep) {
    Query query;
    query.add_steps({Axis::PARENT, "x"_key});

    Object obj = json::parse("{'x': {'y': 'tea'}}");
    auto it = query.iter_eval(obj.get("x"_key).get("y"_key));

    Object it_obj = it.next();
    EXPECT_TRUE(it_obj != nil);
    EXPECT_TRUE(it_obj.is(obj.get("x"_key)));

    it_obj = it.next();
    EXPECT_TRUE(it_obj == nil);
}

TEST(Query, ParentAnyStep) {
    Query query;
    query.add_steps({Axis::PARENT, nil});

    Object obj = json::parse("{'x': 'tea'}");
    auto it = query.iter_eval(obj.get("x"_key));

    Object it_obj = it.next();
    EXPECT_TRUE(it_obj != nil);
    EXPECT_TRUE(it_obj.is(obj));

    it_obj = it.next();
    EXPECT_TRUE(it_obj == nil);
}

TEST(Query, AncestorAnyStep) {
    Query query;
    query.add_steps({Axis::ANCESTOR, nil});

    Object obj = json::parse("{'x': {'y': 'tea'}}");
    Object x = obj.get("x"_key);
    Object y = x.get("y"_key);

    auto it = query.iter_eval(y);

    EXPECT_TRUE(it.next().is(y));
    EXPECT_TRUE(it.next().is(x));
    EXPECT_TRUE(it.next().is(obj));
    EXPECT_TRUE(it.next() == nil);
}

TEST(Query, AncestorStep) {
    Query query;
    query.add_steps({Axis::ANCESTOR, "y"_key});

    Object obj = json::parse("{'x': {'y': {'y': 'tea'}}}");
    Object x = obj.get("x"_key);
    Object y = x.get("y"_key);
    Object yy = y.get("y"_key);

    auto it = query.iter_eval(yy);

    EXPECT_TRUE(it.next().is(yy));
    EXPECT_TRUE(it.next().is(y));
    EXPECT_TRUE(it.next() == nil);
}

TEST(Query, SubtreeAnyStep) {
    Query query;
    query.add_steps({Axis::SUBTREE, nil});

    Object root = json::parse("{'x': [{'u': 'x0u', 'v': 'x0v'}, {'z': 'x1z'}], 'y': {'u': [['xyu00', 'xyu01'], 'xyu1']}}}");

    std::unordered_set<Oid> expect;
    for (auto obj : root.iter_tree()) { expect.insert(obj.id()); }

    auto it = query.iter_eval(root);
    auto count = 0;
    for (auto it_obj = it.next(); it_obj != nil; it_obj = it.next(), ++count) {
        EXPECT_TRUE(expect.contains(it_obj.id()));
    }

    EXPECT_EQ(count, expect.size());
}

TEST(Query, AncestorChild) {
    Object root = json::parse("{'u': {'z': 'uz'}, 'y': {'u': 'yu', 'z': 'yz'}}");

    Query query{Step{Axis::ANCESTOR},
                Step{Axis::CHILD, "u"_key}};

    List actual;
    auto it = query.iter_eval(root.get("y.z"_path));
    for (auto it_obj = it.next(); it_obj != nil; it_obj = it.next()) {
        actual.push_back(it_obj);
    }

    EXPECT_EQ(actual.size(), 2);
    EXPECT_EQ(actual[0], "yu");
    EXPECT_EQ(actual[1].to_str(), R"({"z": "uz"})");
}

TEST(Query, FindFiles) {
    Object wd = bind("file://"_uri);

    auto is_txt_file = [] (const Object& obj) { return obj.key().to_str().ends_with(".txt"); };
    Query query;
    query.add_steps({Axis::SUBTREE, nil, is_txt_file});
    List result = query.eval(wd["test_data"_key]);
    for (auto& obj : result) {
        DEBUG("{}", obj.path());
    }
}
