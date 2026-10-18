/// @file
/// @b License: @n Apache License v2.0
/// @copyright Robert Dunnagan
#include <gtest/gtest.h>
#include <fmt/format.h>
#include <unordered_set>

#include <nodel/core/Object.hxx>
#include <nodel/core/Query.hxx>
#include <nodel/core/Query.hxx>
#include <nodel/core/bind.hxx>
#include <nodel/filesystem.hxx>
#include <nodel/fmt_support.hxx>

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
    auto count = 0UL;
    for (auto it_obj = it.next(); it_obj != nil; it_obj = it.next(), ++count) {
        EXPECT_TRUE(expect.contains(it_obj.id()));
    }

    EXPECT_EQ(count, expect.size());
}

TEST(Query, AncestorChild) {
    Object root = json::parse("{'u': {'z': 'uz'}, 'y': {'u': 'yu', 'z': 'yz'}}");

    Query query{Step{Axis::ANCESTOR},
                Step{Axis::CHILD, "u"_key}};

    ObjectList actual;
    auto it = query.iter_eval(root.get("y.z"_path));
    for (auto it_obj = it.next(); it_obj != nil; it_obj = it.next()) {
        actual.push_back(it_obj);
    }

    EXPECT_EQ(actual.size(), 2UL);
    EXPECT_EQ(actual[0], "yu");
    EXPECT_EQ(actual[1].to_str(), R"({"z": "uz"})");
}

TEST(Query, FindFiles) {
    Object wd = bind("file://?path=."_uri);

    auto is_txt_file = [] (const Object& obj) { return obj.key().to_str().ends_with(".txt"); };
    Query query;
    query.add_steps({Axis::SUBTREE, nil, is_txt_file});
    ObjectList result = query.eval(wd["test_data"_key]);
    std::set<String> found;
    for (auto& obj : result)
        found.insert(obj.path().to_str());
    EXPECT_EQ(found.size(), 2UL);
    std::set<String> expected = {"test_data[\"example.txt\"]", "test_data[\"example.zip\"][\"example.txt\"]"};
    EXPECT_EQ(found, expected);
}

TEST(Query, ChildAnyStepPredicate) {
    Object obj = json::parse("[1, 5, 2, 7]");
    Query query{Step{Axis::CHILD, nil, [] (const Object& o) { return o > 3; }}};
    auto result = query.eval(obj);
    ASSERT_EQ(result.size(), 2UL);
    EXPECT_EQ(result[0], 5);
    EXPECT_EQ(result[1], 7);
}

TEST(Query, SubtreeStepPredicateDescendsIntoNonMatching) {
    Object obj = json::parse("{'a': {'b': {'id': 2}}, 'c': {'id': 2}}");
    auto has_id = [] (const Object& o) { return is_map(o) && o.get("id"_key) != nil; };
    Query query{Step{Axis::SUBTREE, nil, has_id}};
    auto result = query.eval(obj);
    ASSERT_EQ(result.size(), 2UL);
    EXPECT_TRUE(result[0].is(obj.get("a.b"_path)) || result[1].is(obj.get("a.b"_path)));
    EXPECT_TRUE(result[0].is(obj.get("c"_key)) || result[1].is(obj.get("c"_key)));
}

TEST(Query, AncestorStepPredicate) {
    Object obj = json::parse("{'x': {'tag': 1, 'y': {'z': {'tag': 1}}}}");
    Object z = obj.get("x.y.z"_path);
    auto has_tag = [] (const Object& o) { return is_map(o) && o.get("tag"_key) != nil; };
    Query query{Step{Axis::ANCESTOR, nil, has_tag}};
    auto result = query.eval(z);
    ASSERT_EQ(result.size(), 2UL);
    EXPECT_TRUE(result[0].is(z));
    EXPECT_TRUE(result[1].is(obj.get("x"_key)));
}

TEST(Query, ChildStepStringKeyOnList) {
    Object obj = json::parse("{'x': [[1], {'id': 2}]}");
    auto result = "x.*(id=2)"_query.eval(obj);
    ASSERT_EQ(result.size(), 1UL);
    EXPECT_TRUE(result[0].is(obj.get("x[1]"_path)));
}

namespace {
Object query_data() {
    return json::parse(R"({"x": [{"id": 1, "value": 100, "tag": "old"},
                                 {"id": 2, "value": 200, "tag": "new", "on": true},
                                 {"id": 3, "value": 300, "on": false}],
                           "l": [1, 5, 2, 7],
                           "a": [{"tags": [{"name": "t", "v": 4}, {"name": "u", "v": 1}]},
                                 {"tags": [{"name": "t", "v": 2}]}],
                           "y": {"k1": {"id": 2}, "k2": {"id": 3}, "k3": {"z": {"id": 2}}},
                           "in": {"a b": 7}})");
}

std::vector<String> query_ids(const char* spec) {
    std::vector<String> ids;
    for (auto& obj : Query{spec}.eval(query_data()))
        ids.push_back(obj.get("id"_key).to_str());
    return ids;
}

using Ids = std::vector<String>;
}

TEST(Query, ParseEqual) {
    EXPECT_EQ(query_ids("x.*(id=2)"), (Ids{"2"}));
    EXPECT_EQ(query_ids("x.*(id==2)"), (Ids{"2"}));
    EXPECT_EQ(query_ids("x.*( id = 2 )"), (Ids{"2"}));
    EXPECT_EQ(query_ids("x.*(2 = id)"), (Ids{"2"}));
    EXPECT_EQ(query_ids("x.*(tag='new')"), (Ids{"2"}));
    EXPECT_EQ(query_ids("x.*(tag=\"new\")"), (Ids{"2"}));
}

TEST(Query, ParseExample) {
    Object d = json::parse(R"({"x": [{"id": 1, "value": 100}, {"id": 2, "value": 200}]})");
    auto result = "x.*(id=2)"_query.eval(d);
    ASSERT_EQ(result.size(), 1UL);
    EXPECT_TRUE(result[0].is(d.get("x[1]"_path)));
}

TEST(Query, ParseNotEqual) {
    EXPECT_EQ(query_ids("x.*(id!=2)"), (Ids{"1", "3"}));
    EXPECT_EQ(query_ids("x.*(tag!='new')"), (Ids{"1"}));        // missing tag is not compared
    EXPECT_EQ(query_ids("x.*(not tag='new')"), (Ids{"1", "3"}));
}

TEST(Query, ParseOrdering) {
    EXPECT_EQ(query_ids("x.*(id<2)"), (Ids{"1"}));
    EXPECT_EQ(query_ids("x.*(id<=2)"), (Ids{"1", "2"}));
    EXPECT_EQ(query_ids("x.*(id>2)"), (Ids{"3"}));
    EXPECT_EQ(query_ids("x.*(id>=2)"), (Ids{"2", "3"}));
    EXPECT_EQ(query_ids("x.*(value > 150.5)"), (Ids{"2", "3"}));
    EXPECT_EQ(query_ids("x.*(tag < 'old')"), (Ids{"2"}));
}

TEST(Query, ParseOrderingTypeMismatch) {
    EXPECT_EQ(query_ids("x.*(id < 'a')"), (Ids{}));
    EXPECT_EQ(query_ids("x.*(tag > 1)"), (Ids{}));
}

TEST(Query, ParseChainedRange) {
    EXPECT_EQ(query_ids("x.*(1 <= id < 3)"), (Ids{"1", "2"}));
    EXPECT_EQ(query_ids("x.*(1 < id <= 3)"), (Ids{"2", "3"}));
}

TEST(Query, ParseIn) {
    EXPECT_EQ(query_ids("x.*(id in [1, 3])"), (Ids{"1", "3"}));
    EXPECT_EQ(query_ids("x.*(id not in [1, 3])"), (Ids{"2"}));
    EXPECT_EQ(query_ids("x.*(tag in ['new', 'other'])"), (Ids{"2"}));
}

TEST(Query, ParseLogical) {
    EXPECT_EQ(query_ids("x.*(id=1 or id=3)"), (Ids{"1", "3"}));
    EXPECT_EQ(query_ids("x.*(not id=1 and value>=200)"), (Ids{"2", "3"}));
    EXPECT_EQ(query_ids("x.*(id=1 or id=2 and value=100)"), (Ids{"1"}));
    EXPECT_EQ(query_ids("x.*((id=1 or id=2) and value=200)"), (Ids{"2"}));
    EXPECT_EQ(query_ids("x.*(not (id=1 or id=2))"), (Ids{"3"}));
}

TEST(Query, ParseExistence) {
    EXPECT_EQ(query_ids("x.*(tag)"), (Ids{"1", "2"}));
    EXPECT_EQ(query_ids("x.*(not tag)"), (Ids{"3"}));
    EXPECT_EQ(query_ids("x.*(on)"), (Ids{"2"}));          // false is not true
    EXPECT_EQ(query_ids("x.*(on = false)"), (Ids{"3"}));
}

TEST(Query, ParseSelf) {
    auto result = "l.*(. > 3)"_query.eval(query_data());
    ASSERT_EQ(result.size(), 2UL);
    EXPECT_EQ(result[0], 5);
    EXPECT_EQ(result[1], 7);

    Object root = query_data();
    result = "."_query.eval(root);
    ASSERT_EQ(result.size(), 1UL);
    EXPECT_TRUE(result[0].is(root));

    result = ".*(id=2)"_query.eval(root.get("x"_key));
    ASSERT_EQ(result.size(), 1UL);
    EXPECT_EQ(result[0].get("id"_key), 2);
}

TEST(Query, ParseQueryOperand) {
    Object root = query_data();
    auto result = "a.*(tags.*(name='t').v > 3)"_query.eval(root);
    ASSERT_EQ(result.size(), 1UL);
    EXPECT_TRUE(result[0].is(root.get("a[0]"_path)));

    result = "a.*(tags[*].v = 2)"_query.eval(root);
    ASSERT_EQ(result.size(), 1UL);
    EXPECT_TRUE(result[0].is(root.get("a[1]"_path)));

    result = "a.*(.tags[0].name = 't')"_query.eval(root);
    EXPECT_EQ(result.size(), 2UL);
}

TEST(Query, ParseFilterCurrent) {
    Object root = query_data();
    EXPECT_EQ("x(id=2)"_query.eval(root).size(), 0UL);       // x is a list, with no id

    auto result = "x[1](id=2)"_query.eval(root);
    ASSERT_EQ(result.size(), 1UL);
    EXPECT_TRUE(result[0].is(root.get("x[1]"_path)));
    EXPECT_EQ("x[0](id=2)"_query.eval(root).size(), 0UL);

    result = "(l)"_query.eval(root);
    ASSERT_EQ(result.size(), 1UL);
    EXPECT_TRUE(result[0].is(root));
    EXPECT_EQ(".(nope)"_query.eval(root).size(), 0UL);

    EXPECT_EQ(query_ids("x[*](id>1)"), (Ids{"2", "3"}));
    EXPECT_EQ(query_ids("x.*(id>1)(id<3)"), (Ids{"2"}));
    EXPECT_EQ(query_ids("x.*(id>1)(tag)"), (Ids{"2"}));

    result = "x.*(id=2).value"_query.eval(root);
    ASSERT_EQ(result.size(), 1UL);
    EXPECT_EQ(result[0], 200);

    result = "a.*(tags.*(name='u'))"_query.eval(root);
    ASSERT_EQ(result.size(), 1UL);
    EXPECT_TRUE(result[0].is(root.get("a[0]"_path)));
}

TEST(Query, ParseFilterMapValues) {
    Object root = query_data();
    auto result = "y.*(id=2)"_query.eval(root);
    ASSERT_EQ(result.size(), 1UL);
    EXPECT_TRUE(result[0].is(root.get("y.k1"_path)));
}

TEST(Query, ParseWildcards) {
    Object root = query_data();
    EXPECT_EQ("x[*]"_query.eval(root).size(), 3UL);
    EXPECT_EQ("x.*"_query.eval(root).size(), 3UL);
    EXPECT_EQ("x.*.id"_query.eval(root).size(), 3UL);
    EXPECT_EQ("*"_query.eval(root).size(), 5UL);
}

TEST(Query, ParseAxisParent) {
    Object root = query_data();
    auto result = "x[1]:parent"_query.eval(root);
    ASSERT_EQ(result.size(), 1UL);
    EXPECT_TRUE(result[0].is(root.get("x"_key)));

    result = "x[1]:parent:parent"_query.eval(root);
    ASSERT_EQ(result.size(), 1UL);
    EXPECT_TRUE(result[0].is(root));

    EXPECT_EQ(":parent"_query.eval(root).size(), 0UL);
    EXPECT_EQ("x[1]:parent(l)"_query.eval(root).size(), 0UL);
    EXPECT_EQ("x[1]:parent:parent(l)"_query.eval(root).size(), 1UL);
}

TEST(Query, ParseAxisAncestor) {
    Object root = query_data();
    auto result = "y.k3.z:ancestor"_query.eval(root);
    ASSERT_EQ(result.size(), 3UL);
    EXPECT_TRUE(result[0].is(root.get("y.k3"_path)));
    EXPECT_TRUE(result[1].is(root.get("y"_key)));
    EXPECT_TRUE(result[2].is(root));

    result = "y.k3.z:ac(k1)"_query.eval(root);
    ASSERT_EQ(result.size(), 1UL);
    EXPECT_TRUE(result[0].is(root.get("y"_key)));

    EXPECT_EQ(":ac"_query.eval(root).size(), 0UL);
}

TEST(Query, ParseAxisDescendant) {
    Object root = query_data();
    EXPECT_EQ("y:descendant"_query.eval(root).size(), 7UL);
    EXPECT_EQ("y:dc"_query.eval(root).size(), 7UL);
    EXPECT_EQ("l:dc"_query.eval(root).size(), 4UL);
    EXPECT_EQ("l[0]:dc"_query.eval(root).size(), 0UL);

    std::unordered_set<Oid> ids;
    for (auto& obj : ":dc(id=2)"_query.eval(root)) ids.insert(obj.id());
    std::unordered_set<Oid> expect{root.get("x[1]"_path).id(), root.get("y.k1"_path).id(), root.get("y.k3.z"_path).id()};
    EXPECT_EQ(ids, expect);

    // filter applies to the axis, so direct children are included
    auto result = "y:dc(id=3)"_query.eval(root);
    ASSERT_EQ(result.size(), 1UL);
    EXPECT_TRUE(result[0].is(root.get("y.k2"_path)));
}

TEST(Query, ParseAxisChildSelfRoot) {
    Object root = query_data();
    EXPECT_EQ("x:child"_query.eval(root).size(), 3UL);
    EXPECT_EQ(query_ids("x:child(id>1)"), (Ids{"2", "3"}));

    auto result = "x[1]:self(id=2)"_query.eval(root);
    ASSERT_EQ(result.size(), 1UL);
    EXPECT_TRUE(result[0].is(root.get("x[1]"_path)));
    EXPECT_EQ("x[0]:self(id=2)"_query.eval(root).size(), 0UL);

    result = "y.k3.z:root"_query.eval(root);
    ASSERT_EQ(result.size(), 1UL);
    EXPECT_TRUE(result[0].is(root));

    result = ":root.l[3]"_query.eval(root.get("y.k3.z"_path));
    ASSERT_EQ(result.size(), 1UL);
    EXPECT_EQ(result[0], 7);
}

TEST(Query, ParseAxisInPredicate) {
    EXPECT_EQ(query_ids("x.*(:root.l[0] = 1)"), (Ids{"1", "2", "3"}));
    EXPECT_EQ(query_ids("x.*(:parent:parent.l[0] = 2)"), (Ids{}));
    EXPECT_EQ(query_ids("x.*(.:ac(l))"), (Ids{"1", "2", "3"}));
    EXPECT_EQ(query_ids("x.*(:self(id=2))"), (Ids{"2"}));
}

TEST(Query, ParseKeys) {
    Object root = query_data();
    auto result = "x[1].value"_query.eval(root);
    ASSERT_EQ(result.size(), 1UL);
    EXPECT_EQ(result[0], 200);

    result = "x[-1].id"_query.eval(root);
    ASSERT_EQ(result.size(), 1UL);
    EXPECT_EQ(result[0], 3);

    result = "in['a b']"_query.eval(root);   // reserved words are only reserved in predicates
    ASSERT_EQ(result.size(), 1UL);
    EXPECT_EQ(result[0], 7);

    result = "*(['a b'] = 7)"_query.eval(root);
    ASSERT_EQ(result.size(), 1UL);
    EXPECT_TRUE(result[0].is(root.get("in"_key)));
}

TEST(Query, ParseSyntaxErrors) {
    for (auto spec : {"x(", "x(id=)", "x(id=2", "x(and)", "x(id 2)", "x.", "x[abc]", "x[1", "x(id in)", "x(id='a)", "x)",
                      "**", "x.**", "x:", "x:foo", "x:dcx", "x(:bad = 1)"}) {
        EXPECT_THROW(Query{spec}, parse::SyntaxError) << spec;
    }
}

TEST(Query, ParseSyntaxErrorOffset) {
    try {
        Query{"x(id=2 ?)"};
        FAIL();
    } catch (const parse::SyntaxError& err) {
        EXPECT_TRUE(String{err.what()}.find("offset 7") != String::npos) << err.what();
    }
}
