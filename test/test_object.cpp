#include <gtest/gtest.h>
#include <fmt/format.h>
#include <nodel/nodel.h>
#include <algorithm>

#include <nodel/impl/Object.h>
#include <nodel/impl/json.h>

using namespace nodel;

struct TestObject : public Object
{
    TestObject(const Object& object) : Object(object) {}
    TestObject(Object&& object) : Object(std::forward<Object>(object)) {}

    using Object::inc_ref_count;
    using Object::dec_ref_count;
    using Object::m_fields;
    using Object::m_repr;
};

TEST(Object, Empty) {
  Object v;
  EXPECT_TRUE(v.is_empty());
}

TEST(Object, Null) {
  Object v{null};
  EXPECT_TRUE(v.is_null());
  EXPECT_TRUE(v.is(null));
  EXPECT_EQ(v.to_json(), "null");
}

TEST(Object, Bool) {
  Object v{true};
  EXPECT_TRUE(v.is_bool());
  EXPECT_EQ(v.to_json(), "true");

  v = false;
  EXPECT_TRUE(v.is_bool());
  EXPECT_EQ(v.to_json(), "false");
}

TEST(Object, Int64) {
  Object v{-0x7FFFFFFFFFFFFFFFLL};
  EXPECT_TRUE(v.is_int());
  EXPECT_EQ(v.to_json(), "-9223372036854775807");
}

TEST(Object, UInt64) {
  Object v{0xFFFFFFFFFFFFFFFFULL};
  EXPECT_TRUE(v.is_uint());
  EXPECT_EQ(v.to_json(), "18446744073709551615");
}

TEST(Object, Double) {
  Object v{3.141593};
  EXPECT_TRUE(v.is_float());
  EXPECT_EQ(v.to_json(), "3.141593");
}

TEST(Object, String) {
  Object v{"123"};
  EXPECT_TRUE(v.is_str());
  EXPECT_TRUE(v.parent().is_null());
  EXPECT_EQ(v.to_json(), "\"123\"");

  Object quoted{"a\"b"};
  EXPECT_TRUE(quoted.is_str());
  EXPECT_EQ(quoted.to_json(), "\"a\\\"b\"");
}

TEST(Object, List) {
  Object list{List{Object(1), Object("tea"), Object(3.14), Object(true)}};
  EXPECT_TRUE(list.is_list());
  EXPECT_EQ(list.to_json(), "[1, \"tea\", 3.14, true]");
}

TEST(Object, Map) {
  Object map{Map{}};
  EXPECT_TRUE(map.is_map());
}

TEST(Object, MapKeyOrder) {
  Object map{Map{{"x", Object(100)}, {"y", Object("tea")}, {90, Object(true)}}};
  EXPECT_TRUE(map.is_map());
  map.to_json();
//  EXPECT_EQ(map.to_json(), "{\"x\": 100, \"y\": \"tea\", 90: true}");
}

TEST(Object, Size) {
    EXPECT_EQ(Object(null).size(), 0);
    EXPECT_EQ(Object(1LL).size(), 0);
    EXPECT_EQ(Object(1ULL).size(), 0);
    EXPECT_EQ(Object(1.0).size(), 0);
    EXPECT_EQ(Object("foo").size(), 3);
    EXPECT_EQ(parse_json("[1, 2, 3]").size(), 3);
    EXPECT_EQ(parse_json("{'x': 1, 'y': 2}").size(), 2);
}

TEST(Object, ToStr) {
  EXPECT_EQ(Object{null}.to_str(), "null");
  EXPECT_EQ(Object{false}.to_str(), "false");
  EXPECT_EQ(Object{true}.to_str(), "true");
  EXPECT_EQ(Object{7LL}.to_str(), "7");
  EXPECT_EQ(Object{0xFFFFFFFFFFFFFFFFULL}.to_str(), "18446744073709551615");
  EXPECT_EQ(Object{3.14}.to_str(), "3.14");
  EXPECT_EQ(Object{"trivial"}.to_str(), "trivial");
  EXPECT_EQ(parse_json("[1, 2, 3]").to_str(), "[1, 2, 3]");
  EXPECT_EQ(parse_json("{'name': 'Dude'}").to_str(), "{\"name\": \"Dude\"}");

  const char* json = R"({"a": [], "b": [1], "c": [2, 3], "d": [4, [5, 6]]})";
  EXPECT_EQ(parse_json(json).to_str(), json);
}

TEST(Object, IdentityComparison) {
    Object obj{"foo"};
    Object copy{obj};
    Object copy2;
    EXPECT_TRUE(obj.is(copy));
    copy2 = obj;
    EXPECT_TRUE(obj.is(copy2));
    EXPECT_TRUE(copy.is(copy2));
    EXPECT_TRUE(copy2.is(obj));
}

TEST(Object, CompareBoolNull) {
    Object a{false};
    Object b{};
    try {
        EXPECT_FALSE(a == b);
        EXPECT_FALSE(a < b);
        FAIL();
    } catch (...) {
    }
}

TEST(Object, CompareBoolBool) {
  Object a{true};
  Object b{true};
  EXPECT_FALSE(a != b);  // just verifying that != invokes ==
  EXPECT_TRUE(a == b);
  EXPECT_FALSE(a < b);
  EXPECT_FALSE(a > b);
}

TEST(Object, CompareBoolInt) {
    Object a{true};
    Object b{1};
    EXPECT_TRUE(a == b);
    try {
        EXPECT_FALSE(a < b);
        FAIL();
    } catch (...) {
    }

    EXPECT_TRUE(b == a);
    try {
        EXPECT_FALSE(b > a);
        FAIL();
    } catch (...) {
    }
}

TEST(Object, CompareBoolFloat) {
    Object a{true};
    Object b{1.0};
    EXPECT_TRUE(a == b);
    try {
        EXPECT_FALSE(a < b);
        FAIL();
    } catch (...) {
    }

    EXPECT_TRUE(b == a);
    try {
        EXPECT_FALSE(b < a);
        FAIL();
    } catch (...) {
    }
}

TEST(Object, CompareBoolStr) {
    Object a{true};
    Object b{"false"};
    try {
        EXPECT_FALSE(a == b);
        FAIL();
    } catch (...) {
    }
    try {
        EXPECT_TRUE(a > b);
        FAIL();
    } catch (...) {
    }

    try {
        EXPECT_FALSE(b == a);
        FAIL();
    } catch (...) {
    }
    try {
        EXPECT_TRUE(b < a);
        FAIL();
    } catch (...) {
    }
}

TEST(Object, CompareBoolList) {
    Object a{true};
    Object b = parse_json("[1]");
    try {
        EXPECT_FALSE(a == b);
        FAIL();
    } catch (...) {
    }
    try {
        EXPECT_FALSE(a > b);
        FAIL();
    } catch (...) {
    }

    try {
        EXPECT_FALSE(b == a);
        FAIL();
    } catch (...) {
    }
    try {
        EXPECT_FALSE(b < a);
        FAIL();
    } catch (...) {
    }
}

TEST(Object, CompareIntNull) {
    Object a{0};
    Object b{};
    try {
        EXPECT_FALSE(a == b);
        FAIL();
    } catch (...) {
    }
    try {
        EXPECT_FALSE(a > b);
        FAIL();
    } catch (...) {
    }

    try {
        EXPECT_FALSE(b == a);
        FAIL();
    } catch (...) {
    }
    try {
        EXPECT_FALSE(b < a);
        FAIL();
    } catch (...) {
    }
}

TEST(Object, CompareIntInt) {
  Object a{1};
  Object b{2};
  EXPECT_TRUE(a != b);
  EXPECT_TRUE(a < b);
  EXPECT_FALSE(a > b);
}

TEST(Object, CompareIntUInt) {
  Object a{1};
  Object b{0xFFFFFFFFFFFFFFFFULL};
  EXPECT_TRUE(a != b);
  EXPECT_TRUE(a < b);
  EXPECT_FALSE(a > b);

  EXPECT_TRUE(b != a);
  EXPECT_TRUE(b > a);
  EXPECT_FALSE(b < a);
}

TEST(Object, CompareIntFloat) {
  Object a{1};
  Object b{1.0};
  EXPECT_TRUE(a == b);
  EXPECT_FALSE(a < b);
  EXPECT_FALSE(a > b);

  EXPECT_TRUE(b == a);
  EXPECT_FALSE(b < a);
  EXPECT_FALSE(b > a);

  Object c{1.1};
  EXPECT_FALSE(a == c);
  EXPECT_TRUE(a < c);
  EXPECT_FALSE(a > c);

  EXPECT_FALSE(c == a);
  EXPECT_TRUE(c > a);
  EXPECT_FALSE(c < a);
}

TEST(Object, CompareIntStr) {
    Object a{1};
    Object b{"1"};
    try {
        EXPECT_FALSE(a == b);
        FAIL();
    } catch (...) {
    }
    Object c{"0"};
    try {
        EXPECT_TRUE(a > c);
        FAIL();
    } catch (...) {
    }

    try {
        EXPECT_FALSE(b == a);
        FAIL();
    } catch (...) {
    }
    try {
        EXPECT_TRUE(c < a);
        FAIL();
    } catch (...) {
    }
}

TEST(Object, CompareIntList) {
    Object a{1};
    Object b = parse_json("[1]");
    try {
        EXPECT_FALSE(a == b);
        FAIL();
    } catch (...) {
    }
    try {
        EXPECT_FALSE(a > b);
        FAIL();
    } catch (...) {
    }

    try {
        EXPECT_FALSE(b == a);
        FAIL();
    } catch (...) {
    }
    try {
        EXPECT_FALSE(b < a);
        FAIL();
    } catch (...) {
    }
}

TEST(Object, CompareIntMap) {
    Object a{1};
    Object b = parse_json("{}");
    EXPECT_TRUE(b.is_map());
    try {
        EXPECT_FALSE(a == b);
        FAIL();
    } catch (...) {
    }
    try {
        EXPECT_FALSE(a > b);
        FAIL();
    } catch (...) {
    }

    try {
        EXPECT_FALSE(b == a);
        FAIL();
    } catch (...) {
    }
    try {
        EXPECT_FALSE(b < a);
        FAIL();
    } catch (...) {
    }
}

TEST(Object, CompareUIntFloat) {
  Object a{0xFFFFFFFFFFFFFFFFULL};
  Object b{1e100};
  EXPECT_TRUE(a != b);
  EXPECT_TRUE(a < b);
  EXPECT_FALSE(a > b);

  EXPECT_TRUE(b != a);
  EXPECT_TRUE(b > a);
  EXPECT_FALSE(b < a);
}

TEST(Object, CompareUIntStr) {
    Object a{1ULL};
    Object b{"1"};
    try {
        EXPECT_FALSE(a == b);
        FAIL();
    } catch (...) {
    }
    Object c{"0"};
    try {
        EXPECT_TRUE(a > c);
        FAIL();
    } catch (...) {
    }

    try {
        EXPECT_FALSE(b == a);
        FAIL();
    } catch (...) {
    }
    try {
        EXPECT_TRUE(c < a);
        FAIL();
    } catch (...) {
    }
}

TEST(Object, CompareUIntList) {
    Object a{1ULL};
    Object b = parse_json("[1]");
    try {
        EXPECT_FALSE(a == b);
        FAIL();
    } catch (...) {
    }
    try {
        EXPECT_FALSE(a > b);
        FAIL();
    } catch (...) {
    }

    try {
        EXPECT_FALSE(b == a);
        FAIL();
    } catch (...) {
    }
    try {
        EXPECT_FALSE(b < a);
        FAIL();
    } catch (...) {
    }
}

TEST(Object, CompareUIntMap) {
    Object a{1ULL};
    Object b = parse_json("{}");
    EXPECT_TRUE(b.is_map());
    try {
        EXPECT_FALSE(a == b);
        FAIL();
    } catch (...) {
    }
    try {
        EXPECT_FALSE(a > b);
        FAIL();
    } catch (...) {
    }

    try {
        EXPECT_FALSE(b == a);
        FAIL();
    } catch (...) {
    }
    try {
        EXPECT_FALSE(b < a);
        FAIL();
    } catch (...) {
    }
}

TEST(Object, CompareStrStr) {
    Object a{"aaa"};
    Object b{"aba"};
    Object c{"aaa"};
    EXPECT_TRUE(a < b);
    EXPECT_TRUE(a == c);
}

TEST(Object, CompareStrList) {
    Object a{"[1]"};
    Object b = parse_json("[1]");
    try {
        EXPECT_TRUE(a == b);
        FAIL();
    } catch (...) {
    }
    try {
        EXPECT_FALSE(a > b);
        FAIL();
    } catch (...) {
    }

    try {
        EXPECT_TRUE(b == a);
        FAIL();
    } catch (...) {
    }
    try {
        EXPECT_FALSE(b < a);
        FAIL();
    } catch (...) {
    }
}

TEST(Object, CompareStrMap) {
    Object a{"{}"};
    Object b = parse_json("{}");
    EXPECT_TRUE(b.is_map());
    try {
        EXPECT_TRUE(a == b);
        FAIL();
    } catch (...) {
    }
    try {
        EXPECT_FALSE(a > b);
        FAIL();
    } catch (...) {
    }

    try {
        EXPECT_TRUE(b == a);
        FAIL();
    } catch (...) {
    }
    try {
        EXPECT_FALSE(b < a);
        FAIL();
    } catch (...) {
    }
}

TEST(Object, ToKey) {
  Object obj{"key"};
  EXPECT_TRUE(obj.is_str());
  EXPECT_EQ(obj.to_key().as<String>(), "key");
}

TEST(Object, ListGet) {
  Object obj = parse_json("[7, 8, 9]");
  EXPECT_TRUE(obj.is_list());
  EXPECT_EQ(obj.get(0).to_int(), 7);
  EXPECT_EQ(obj.get(1).to_int(), 8);
  EXPECT_EQ(obj.get(2).to_int(), 9);
  EXPECT_EQ(obj.get(-1).to_int(), 9);
  EXPECT_EQ(obj.get(-2).to_int(), 8);
  EXPECT_EQ(obj.get(-3).to_int(), 7);
  EXPECT_TRUE(obj.get(-4).is_null());
  EXPECT_TRUE(obj.get(-5).is_null());
  EXPECT_TRUE(obj.get(3).is_null());
  EXPECT_TRUE(obj.get(4).is_null());
}

TEST(Object, ListGetOutOfRange) {
    Object obj = parse_json(R"([])");
    EXPECT_TRUE(obj.is_list());
    EXPECT_TRUE(obj.get(1).is_null());
}

TEST(Object, MapGet) {
  Object obj = parse_json(R"({0: 7, 1: 8, 2: 9, "name": "Brian"})");
  EXPECT_TRUE(obj.is_map());
  EXPECT_EQ(obj.get(0).to_int(), 7);
  EXPECT_EQ(obj.get(1).to_int(), 8);
  EXPECT_EQ(obj.get(2).to_int(), 9);
  EXPECT_EQ(obj.get("name").as<String>(), "Brian");
  EXPECT_EQ(obj.get("name"s).as<String>(), "Brian");
  EXPECT_TRUE(obj.get("blah"s).is_null());
}

TEST(Object, MapGetNotFound) {
    Object obj = parse_json(R"({})");
    EXPECT_TRUE(obj.is_map());
    EXPECT_TRUE(obj.get("x").is_null());

    obj.set("x", "X");
    EXPECT_FALSE(obj.get("x").is_null());

    obj.del("x");
    EXPECT_TRUE(obj.get("x").is_null());
}

TEST(Object, MultipleSubscriptMap) {
  Object obj = parse_json(R"({"a": {"b": {"c": 7}}})");
  EXPECT_TRUE(obj.is_map());
  EXPECT_EQ(obj.get("a").get("b").get("c"), 7);
}

TEST(Object, MapSetNumber) {
    Object obj = parse_json("{'x': 100}");
    obj.set("x", 101);
    EXPECT_EQ(obj.get("x"s), 101);
}

TEST(Object, MapSetString) {
    Object obj = parse_json("{'x': ''}");
    obj.set("x", "salmon");
    EXPECT_EQ(obj.get("x"s), "salmon");
}

TEST(Object, MapSetList) {
    Object obj = parse_json("{'x': [100]}");
    Object rhs = parse_json("[101]");
    obj.set("x", rhs);
    EXPECT_EQ(obj.get("x"s).get(0), 101);
}

TEST(Object, MapSetMap) {
    Object obj = parse_json("{'x': [100]}");
    Object rhs = parse_json("{'y': 101}");
    obj.set("x"s, rhs);
    EXPECT_TRUE(obj.get("x"s).is_map());
    EXPECT_EQ(obj.get("x"s).get("y"s), 101);
}

TEST(Object, KeyOf) {
    Object obj = parse_json(
        R"({"bool": true, 
            "int": 1, 
            "uint": 18446744073709551615, 
            "float": 3.1415926, 
            "str": "Assam Tea", 
            "list": [1],
            "map": {"list": [1]},
            "redun_bool": true
            "redun_int": 1,
            "redun_uint": 18446744073709551615,
            "redun_float": 3.1415926,
            "okay_str": "Assam Tea"
           })");
    EXPECT_TRUE(Object{null}.key_of(obj).is_null());
    EXPECT_EQ(obj.key_of(obj.get("bool")), "bool");
    EXPECT_EQ(obj.key_of(obj.get("int")), "int");
    EXPECT_EQ(obj.key_of(obj.get("uint")), "uint");
    EXPECT_EQ(obj.key_of(obj.get("float")), "float");
    EXPECT_EQ(obj.key_of(obj.get("str")), "str");
    EXPECT_EQ(obj.key_of(obj.get("list")), "list");
    EXPECT_EQ(obj.key_of(obj.get("map")), "map");
    EXPECT_EQ(obj.key_of(obj.get("redun_bool")), "bool");
    EXPECT_EQ(obj.key_of(obj.get("redun_int")), "int");
    EXPECT_EQ(obj.key_of(obj.get("redun_uint")), "uint");
    EXPECT_EQ(obj.key_of(obj.get("redun_float")), "float");
    EXPECT_EQ(obj.key_of(obj.get("okay_str")), "okay_str");
}

TEST(Object, LineageRange) {
    Object obj = parse_json(R"({"a": {"b": ["Assam", "Ceylon"]}})");
    List ancestors;
    for (auto anc : obj.get("a").get("b").get(1).iter_line())
        ancestors.push_back(anc);
    EXPECT_EQ(ancestors.size(), 4);
    EXPECT_TRUE(ancestors[0].is(obj.get("a").get("b").get(1)));
    EXPECT_TRUE(ancestors[1].is(obj.get("a").get("b")));
    EXPECT_TRUE(ancestors[2].is(obj.get("a")));
    EXPECT_TRUE(ancestors[3].is(obj));
}

TEST(Object, ListChildrenRange) {
    Object obj = parse_json(R"([true, 1, "x"])");
    List children;
    for (auto obj : obj.values())
        children.push_back(obj);
    EXPECT_EQ(children.size(), 3);
    EXPECT_TRUE(children[0].is_bool());
    EXPECT_TRUE(children[1].is_int());
    EXPECT_TRUE(children[2].is_str());
}

TEST(Object, OrderedMapChildrenRange) {
    Object obj = parse_json(R"({"a": true, "b": 1, "c": "x"})");
    List children;
    for (auto obj : obj.values())
        children.push_back(obj);
    EXPECT_EQ(children.size(), 3);
    EXPECT_TRUE(children[0].is_bool());
    EXPECT_TRUE(children[1].is_int());
    EXPECT_TRUE(children[2].is_str());
}

TEST(Object, TreeRange) {
    Object obj = parse_json(R"({
        "a": {"aa": "AA", "ab": "AB"}, 
        "b": [{"b0a": "B0A", "b0b": "B0B"}, 
              {"b1a": "B1A", "b1b": ["B1B0"], "b1c": {"b1ca": "B1CA"}},
              [], "B2"]
    })");
    List list;
    for (auto des : obj.iter_tree())
        list.push_back(des);
    ASSERT_EQ(list.size(), 16);
    EXPECT_TRUE(list[0].is(obj));
    EXPECT_TRUE(list[1].is(obj.get("a")));
    EXPECT_TRUE(list[7].is(obj.get("b").get(2)));
    EXPECT_TRUE(list[8].is(obj.get("b").get(3)));
    EXPECT_EQ(list[9], "B0A");
    EXPECT_EQ(list[15], "B1CA");
}

TEST(Object, TreeRange_VisitPred) {
    Object obj = parse_json(R"({
        "a": {"aa": "AA", "ab": "AB"}, 
        "b": [{"b0a": "B0A", "b0b": "B0B"}, 
              {"b1a": "B1A", "b1b": ["B1B0"], "b1c": {"b1ca": "B1CA"}},
              [], "B2"]
    })");
    List list;
    auto pred = [](const Object& o) { return o.is_type<String>() && o.as<String>()[0] == 'B'; };
    for (auto des : obj.iter_tree(pred))
        list.push_back(des);
    ASSERT_EQ(list.size(), 6);
    EXPECT_TRUE(list[0].is(obj.get("b").get(3)));
    EXPECT_TRUE(list[1].is(obj.get("b").get(0).get("b0a")));
    EXPECT_TRUE(list[5].is(obj.get("b").get(1).get("b1c").get("b1ca")));
}

TEST(Object, TreeRange_EnterPred) {
    Object obj = parse_json(R"({
        "a": {"aa": "AA", "ab": "AB"}, 
        "b": [{"b0a": "B0A", "b0b": "B0B"}, 
              {"b1a": "B1A", "b1b": ["B1B0"], "b1c": {"b1ca": "B1CA"}},
              [], "B2"]
    })");
    List list;
    auto pred = [](const Object& o) { return o.is_type<Map>(); };
    for (auto des : obj.iter_tree_if(pred))
        list.push_back(des);
    ASSERT_EQ(list.size(), 5);
    EXPECT_TRUE(list[0].is(obj));
    EXPECT_TRUE(list[1].is(obj.get("a")));
    EXPECT_TRUE(list[2].is(obj.get("b")));
    EXPECT_TRUE(list[3].is(obj.get("a").get("aa")));
    EXPECT_TRUE(list[4].is(obj.get("a").get("ab")));
}

TEST(Object, TreeRange_VisitAndEnterPred) {
    Object obj = parse_json(R"({
        "a": {"aa": "AA", "ab": "AB"}, 
        "b": [{"b0a": "B0A", "b0b": "B0B"}, 
              {"b1a": "B1A", "b1b": ["B1B0"], "b1c": {"b1ca": "B1CA"}},
              [], "B2"]
    })");
    List list;
    auto visit_pred = [](const Object& o) { return o.is_type<String>(); };
    auto enter_pred = [](const Object& o) { return o.is_type<Map>(); };
    for (auto des : obj.iter_tree_if(visit_pred, enter_pred))
        list.push_back(des);
    ASSERT_EQ(list.size(), 2);
    EXPECT_EQ(list[0], "AA");
    EXPECT_EQ(list[1], "AB");
}

TEST(Object, ValuesRangeMultiuser) {
    Object o1 = parse_json(R"({"a": "A", "b": "B", "c": "C"})");
    Object o2 = parse_json(R"({"x": "X", "y": "Y", "z": "Z"})");
    List result;
    auto r1 = o1.values();
    auto it1 = r1.begin();
    auto end1 = r1.end();
    auto r2 = o2.values();
    auto it2 = r2.begin();
    auto end2 = r2.end();
    for (; it1 != end1 && it2 != end2; ++it1, ++it2) {
        result.push_back(*it1);
        result.push_back(*it2);
    }

    std::vector<std::string> expect = {"A", "X", "B", "Y", "C", "Z"};
    EXPECT_EQ(result.size(), expect.size());
    for (int i=0; i<result.size(); ++i) {
        EXPECT_EQ(result[i], expect[i]);
    }
}

TEST(Object, GetPath) {
    Object obj = parse_json(R"({"a": {"b": ["Assam", "Ceylon"]}})");
    EXPECT_EQ(obj.get("a").path().to_str(), ".a");
    EXPECT_EQ(obj.get("a").get("b").path().to_str(), ".a.b");
    EXPECT_EQ(obj.get("a").get("b").get(1).path().to_str(), ".a.b[1]");
    EXPECT_EQ(obj.get("a").get("b").get(0).path().to_str(), ".a.b[0]");
    OPath path = obj.get("a").get("b").get(1).path();
    EXPECT_EQ(obj.get(path).id(), obj.get("a").get("b").get(1).id());
}

TEST(Object, CreatePath) {
    Object obj = parse_json("{}");

    OPath path;
    path.append("a");
    path.append(0);
    path.append("b");
    path.create(obj, 100);

    EXPECT_EQ(path.to_str(), ".a[0].b");
    EXPECT_TRUE(obj.get("a").is_list());
    EXPECT_EQ(obj.get("a").get(0).type(), Object::OMAP_I);
    EXPECT_EQ(obj.get("a").get(0).get("b"), 100);
}

TEST(Object, CreatePartialPath) {
    Object obj = parse_json("{'a': {}}");

    OPath path;
    path.append("a");
    path.append("b");
    path.append(0);
    path.create(obj, 100);

    EXPECT_EQ(path.to_str(), ".a.b[0]");
    EXPECT_TRUE(obj.get("a").is_map());
    EXPECT_TRUE(obj.get("a").get("b").is_list());
    EXPECT_EQ(obj.get("a").get("b").get(0), 100);
}

//TEST(Object, ParsePath) {
//    Path path{"a.b[1].c[2][3].d"};
//    KeyList keys = {"a", "b", 1, "c", 2, 3, "d"};
//    EXPECT_EQ(path.keys(), keys);
//}

TEST(Object, ParentUpdateRefCount) {
    Object o1 = parse_json("{'x': 'X'}");
    Object x = o1.get("x");
    EXPECT_EQ(x.ref_count(), 2);
    Object o2 = parse_json("{}");
    o2.set('x', x);
    EXPECT_EQ(x.ref_count(), 2);
}

TEST(Object, CopyCtorRefCountIntegrity) {
    Object obj = parse_json("{}");
    EXPECT_EQ(obj.ref_count(), 1);
    Object copy{obj};
    EXPECT_EQ(obj.ref_count(), 2);
    EXPECT_EQ(copy.ref_count(), 2);
}

TEST(Object, MoveCtorRefCountIntegrity) {
    Object obj = parse_json("{}");
    Object move{std::move(obj)};
    EXPECT_EQ(move.ref_count(), 1);
    EXPECT_TRUE(obj.is_empty());
    EXPECT_EQ(obj.ref_count(), Object::no_ref_count);
}

TEST(Object, WalkDF) {
  Object obj = parse_json("[1, [2, [3, [4, 5], 6], 7], 8]");
  std::vector<Int> expect_order{1, 2, 3, 4, 5, 6, 7, 8};
  std::vector<Int> actual_order;

  auto visitor = [&actual_order] (const Object& parent, const Key& key, const Object& object, int) -> void {
    if (!object.is_container())
      actual_order.push_back(object.to_int());
  };

  WalkDF walk{obj, visitor};
  while (walk.next()) {}

  EXPECT_EQ(actual_order.size(), expect_order.size());
  EXPECT_EQ(actual_order, expect_order);
}

TEST(Object, WalkBF) {
  Object obj = parse_json("[1, [2, [3, [4, 5], 6], 7], 8]");
  std::vector<Int> expect_order{1, 8, 2, 7, 3, 6, 4, 5};
  std::vector<Int> actual_order;

  auto visitor = [&actual_order] (const Object& parent, const Key& key, const Object& object) -> void {
    if (!object.is_container())
      actual_order.push_back(object.to_int());
  };

  WalkBF walk{obj, visitor};
  while (walk.next()) {}

  EXPECT_EQ(actual_order.size(), expect_order.size());
  EXPECT_EQ(actual_order, expect_order);
}

static void dump(const char* data) {
    for (auto* p = data; *p != 0 ; p++) {
        fmt::print("{:x}", *p);
    }
    std::cout << std::endl;
}

TEST(Object, RefCountPrimitive) {
  Object obj{7};
  EXPECT_TRUE(obj.is_int());
  EXPECT_EQ(obj.ref_count(), Object::no_ref_count);
}

TEST(Object, RefCountNewString) {
  Object obj{"etc"};
  EXPECT_TRUE(obj.is_str());
  EXPECT_EQ(obj.ref_count(), 1);
}

TEST(Object, RefCountNewList) {
  Object obj{List{}};
  EXPECT_TRUE(obj.is_list());
  EXPECT_EQ(obj.ref_count(), 1);
}

TEST(Object, RefCountNewMap) {
  Object obj{Map{}};
  EXPECT_TRUE(obj.is_map());
  EXPECT_EQ(obj.ref_count(), 1);
}

TEST(Object, RefCountCopy) {
  Object obj{"etc"};
  Object copy1{obj};
  Object copy2{obj};
  Object* copy3 = new Object(copy2);
  EXPECT_TRUE(obj.is_str());
  EXPECT_TRUE(copy1.is_str());
  EXPECT_TRUE(copy2.is_str());
  EXPECT_TRUE(copy3->is_str());
  EXPECT_EQ(obj.ref_count(), 4);
  EXPECT_EQ(copy1.ref_count(), 4);
  EXPECT_EQ(copy2.ref_count(), 4);
  EXPECT_EQ(copy3->ref_count(), 4);
  delete copy3;
  EXPECT_EQ(obj.ref_count(), 3);
  EXPECT_EQ(copy1.ref_count(), 3);
  EXPECT_EQ(copy2.ref_count(), 3);
}

TEST(Object, RefCountMove) {
  Object obj{"etc"};
  Object copy{std::move(obj)};
  EXPECT_TRUE(obj.is_empty());
  EXPECT_TRUE(copy.is_str());
  EXPECT_EQ(obj.ref_count(), Object::no_ref_count);
  EXPECT_EQ(copy.ref_count(), 1);
}

TEST(Object, RefCountCopyAssign) {
  Object obj{"etc"};
  Object copy1; copy1 = obj;
  Object copy2; copy2 = obj;
  Object* copy3 = new Object(); *copy3 = copy2;
  EXPECT_TRUE(obj.is_str());
  EXPECT_TRUE(copy1.is_str());
  EXPECT_TRUE(copy2.is_str());
  EXPECT_TRUE(copy3->is_str());
  EXPECT_EQ(obj.ref_count(), 4);
  EXPECT_EQ(copy1.ref_count(), 4);
  EXPECT_EQ(copy2.ref_count(), 4);
  EXPECT_EQ(copy3->ref_count(), 4);
  delete copy3;
  EXPECT_EQ(obj.ref_count(), 3);
  EXPECT_EQ(copy1.ref_count(), 3);
  EXPECT_EQ(copy2.ref_count(), 3);
}

TEST(Object, RefCountMoveAssign) {
  Object obj{"etc"};
  Object copy;
  copy = std::move(obj);
  EXPECT_TRUE(obj.is_empty());
  EXPECT_TRUE(copy.is_str());
  EXPECT_EQ(obj.ref_count(), Object::no_ref_count);
  EXPECT_EQ(copy.ref_count(), 1);
}

TEST(Object, RefCountTemporary) {
  Object obj{Object{"etc"}};
  EXPECT_TRUE(obj.is_str());
  EXPECT_EQ(obj.ref_count(), 1);
}

template <typename T>
void ptr_alignment_requirement_test() {
  std::vector<T*> ptrs;
  for (int i=0; i<1000; i++) {
    auto ptr = new T{};
    ptrs.push_back(ptr);
    EXPECT_TRUE(((uint64_t)ptr & 0x1) == 0);
  }
  for (auto ptr : ptrs) {
    delete ptr;
  }
}

TEST(Object, StringPtrAlignmentRequirement) {
  ptr_alignment_requirement_test<IRCString>();
}

TEST(Object, ListPtrAlignmentRequirement) {
  ptr_alignment_requirement_test<IRCList>();
}

TEST(Object, MapPtrAlignmentRequirement) {
  ptr_alignment_requirement_test<IRCMap>();
}

TEST(Object, PtrIDEquality) {
  Object obj{"etc"};
  Object copy{obj};
  EXPECT_EQ(obj.id(), copy.id());
}

TEST(Object, NumericIDEquality) {
  Object obj{717};
  Object copy{obj};
  Object other{718};
  EXPECT_EQ(obj.id(), copy.id());
  EXPECT_NE(obj.id(), other.id());
}

TEST(Object, MaxNumericIDEquality) {
  Object obj{0xFFFFFFFFFFFFFFFFULL};
  Object copy{obj};
  Object other{0x7FFFFFFFFFFFFFFFULL};
  EXPECT_EQ(obj.id(), copy.id());
  EXPECT_NE(obj.id(), other.id());
}

TEST(Object, ZeroNullPtrIDConflict) {
  Object obj{null};
  Object num{0};
  EXPECT_NE(obj.id(), num.id());
}

TEST(Object, AssignNull) {
    Object obj{"foo"};
    EXPECT_EQ(obj.ref_count(), 1);
    obj = null;
    EXPECT_TRUE(obj.is_null());
}

TEST(Object, AssignBool) {
    Object obj{"foo"};
    obj = true;
    EXPECT_TRUE(obj.is_bool());
}

TEST(Object, AssignInt32) {
    Object obj{"foo"};
    obj = (int32_t)1;
    EXPECT_TRUE(obj.is_int());
}

TEST(Object, AssignInt64) {
    Object obj{"foo"};
    obj = (int64_t)1;
    EXPECT_TRUE(obj.is_int());
}

TEST(Object, AssignUInt32) {
    Object obj{"foo"};
    obj = (uint32_t)1;
    EXPECT_TRUE(obj.is_uint());
}

TEST(Object, AssignUInt64) {
    Object obj{"foo"};
    obj = (uint64_t)1;
    EXPECT_TRUE(obj.is_uint());
}

TEST(Object, AssignString) {
    Object obj{7};
    obj = "foo"sv;
    EXPECT_TRUE(obj.is_str());
}

TEST(Object, RedundantAssign) {
    Object obj = parse_json(R"({"x": [1], "y": [2]})");
    EXPECT_TRUE(obj.get("x").is_list());
    EXPECT_EQ(obj.get("x").get(0), 1);
    // ref count error might not manifest with one try
    for (int i=0; i<100; i++) {
        obj.get("x") = obj.get("x");
        EXPECT_TRUE(obj.get("x").is_list());
        ASSERT_EQ(obj.get("x").get(0), 1);
    }
}

TEST(Object, RootParentIsNull) {
    Object root = parse_json(R"({"x": [1], "y": [2]})");
    EXPECT_TRUE(root.parent().is_null());
}

TEST(Object, ClearParentOnListUpdate) {
    Object root{Object::LIST_I};
    root.set(0, std::to_string(0));

    Object first = root.get(0);
    EXPECT_TRUE(first.parent().is(root));

    root.set(0, null);
    EXPECT_TRUE(first.parent().is_null());
}

TEST(Object, ClearParentOnMapUpdate) {
    Object root{Object::OMAP_I};
    root.set("0", std::to_string(0));

    Object first = root.get("0");
    EXPECT_TRUE(first.parent().is(root));

    root.set("0", null);
    EXPECT_TRUE(first.parent().is_null());
}

TEST(Object, CopyChildToAnotherContainer) {
    Object m1 = parse_json(R"({"x": "m1x"})");
    Object m2 = parse_json(R"({})");
    m2.set("x", m1);
    EXPECT_TRUE(m1.get("x").parent().is(m1));
    EXPECT_TRUE(m2.get("x").parent().is(m2));
    m2.set("x", "m2x");
    EXPECT_EQ(m2.get("x"), "m2x");
}

TEST(Object, DeepCopyChildToAnotherContainer) {
    Object m1 = parse_json(R"({"x": ["m1x0", "m1x1"]})");
    Object m2 = parse_json(R"({})");
    m2.set("x", m1.get("x"));
    EXPECT_TRUE(m1.get("x").get(1).root().is(m1));
    EXPECT_TRUE(m2.get("x").get(1).root().is(m2));
    m2.get("x").set(1, "m2x1");
    EXPECT_EQ(m1.get("x").get(1), "m1x1");
    EXPECT_EQ(m2.get("x").get(1), "m2x1");
}

TEST(Object, ParentIntegrityOnDel) {
    Object par = parse_json(R"({"x": [1], "y": [2]})");
    Object x1 = par.get("x");
    Object x2 = par.get("x");
    EXPECT_TRUE(x1.parent().is_map());
    EXPECT_EQ(x2.parent().id(), par.id());
    EXPECT_TRUE(x1.parent().is_map());
    EXPECT_EQ(x2.parent().id(), par.id());
    par.del("x"s);
    EXPECT_TRUE(x1.parent().is_null());
    EXPECT_TRUE(x2.parent().is_null());
}

TEST(Object, GetKeys) {
    Object obj = parse_json(R"({"x": [1], "y": [2]})");
    KeyList expect = {"x", "y"};
    EXPECT_EQ(obj.keys(), expect);
}


struct TestSimpleSource : public DataSource
{
    TestSimpleSource(const std::string& json, Mode mode = Mode::READ | Mode::WRITE | Mode::OVERWRITE)
      : DataSource(Kind::COMPLETE, mode, Origin::SOURCE), data{parse_json(json)} {}

    DataSource* copy(const Object& target, Origin origin) const override { return new TestSimpleSource(data.to_json()); }

    void read_meta(const Object&, Object& cache) override {
        read_meta_called = true;
        std::istringstream in{data.to_json()};
        json::impl::Parser parser{in};
        cache = Object{(Object::ReprType)parser.parse_type()};
    }

    void read(const Object&, Object& cache) override                    { read_called = true; cache = data; }
    void write(const Object&, const Object& cache, bool quiet) override { write_called = true; data = cache; }

    Object data;
    bool read_meta_called = false;
    bool read_called = false;
    bool write_called = false;
};


struct TestSparseSource : public DataSource
{
    TestSparseSource(const std::string& json, Mode mode = Mode::READ | Mode::WRITE | Mode::OVERWRITE)
      : DataSource(Kind::SPARSE, mode, Origin::SOURCE), data{parse_json(json)} {}

    DataSource* copy(const Object& target, Origin origin) const override { return new TestSparseSource(data.to_json()); }

    void read_meta(const Object&, Object& cache) override {
        read_meta_called = true;
        std::istringstream in{data.to_json()};
        json::impl::Parser parser{in};
        cache = Object{(Object::ReprType)parser.parse_type()};
    }

    void read(const Object&, Object& cache) override                    { read_called = true; cache = data; }
    void write(const Object&, const Object& cache, bool quiet) override { write_called = true; data = cache; }

    Object read_key(const Object&, const Key& k) override                             { read_key_called = true; return data.get(k); }
    size_t read_size(const Object&) override                                          { return data.size(); }
    void write_key(const Object&, const Key& k, const Object& v, bool quiet) override { write_key_called = true; data.set(k, v); }
    void write_key(const Object&, const Key& k, Object&& v, bool quiet) override      { write_key_called = true; data.set(k, std::forward<Object>(v)); }

    class TestKeyIterator : public KeyIterator
    {
      public:
        TestKeyIterator(TestSparseSource& ds, const Object& data) : m_ds{ds} {
            auto range = data.iter_keys();
            m_it = range.begin();
            m_end = range.end();
        }

        ~TestKeyIterator() { m_ds.iter_deleted = true; }

        bool next_impl() override { if (m_it == m_end) { return false; } else { m_key = *m_it; ++m_it; return true; } }

      private:
        TestSparseSource& m_ds;
        nodel::KeyIterator m_it;
        nodel::KeyIterator m_end;
    };

    class TestValueIterator : public ValueIterator
    {
      public:
        TestValueIterator(TestSparseSource& ds, const Object& data) : m_ds{ds} {
            auto range = data.iter_values();
            m_it = range.begin();
            m_end = range.end();
        }

        ~TestValueIterator() { m_ds.iter_deleted = true; }

        bool next_impl() override { if (m_it == m_end) { return false; } else { m_value = *m_it; ++m_it; return true; } }

      private:
        TestSparseSource& m_ds;
        nodel::ValueIterator m_it;
        nodel::ValueIterator m_end;
    };

    class TestItemIterator : public ItemIterator
    {
      public:
        TestItemIterator(TestSparseSource& ds, const Object& data) : m_ds{ds} {
            auto range = data.iter_items();
            m_it = range.begin();
            m_end = range.end();
        }

        ~TestItemIterator() { m_ds.iter_deleted = true; }

        bool next_impl() override { if (m_it == m_end) { return false; } else { m_item = *m_it; ++m_it; return true; } }

      private:
        TestSparseSource& m_ds;
        nodel::ItemIterator m_it;
        nodel::ItemIterator m_end;
    };

    std::unique_ptr<KeyIterator> key_iter() override     { return std::make_unique<TestKeyIterator>(*this, data); }
    std::unique_ptr<ValueIterator> value_iter() override { return std::make_unique<TestValueIterator>(*this, data); }
    std::unique_ptr<ItemIterator> item_iter() override   { return std::make_unique<TestItemIterator>(*this, data); }

    using DataSource::m_cache;

    Object data;
    bool read_meta_called = false;
    bool read_called = false;
    bool write_called = false;
    bool read_key_called = false;
    bool write_key_called = false;
    bool iter_deleted = false;
};


TEST(Object, TestSimpleSource_RefCountCopyAssign) {
    Object obj{new TestSimpleSource(R"("foo")")};
    EXPECT_EQ(obj.ref_count(), 1);
    Object copy = obj;
    EXPECT_EQ(obj.ref_count(), 2);
    EXPECT_EQ(copy.ref_count(), 2);
    obj.release();
    EXPECT_EQ(copy.ref_count(), 1);
}

TEST(Object, TestSimpleSource_RefCountMoveAssign) {
    Object obj{new TestSimpleSource(R"("foo")")};
    EXPECT_EQ(obj.ref_count(), 1);
    Object copy = std::move(obj);
    EXPECT_EQ(copy.ref_count(), 1);
    obj.release();
    EXPECT_EQ(copy.ref_count(), 1);
}

TEST(Object, TestSimpleSource_GetType) {
    Object obj{new TestSimpleSource(R"({"x": 1, "y": 2})")};
    EXPECT_TRUE(obj.is_map());
}

TEST(Object, TestSimpleSource_ChildParent) {
    Object obj{new TestSimpleSource(R"({"x": "X"})")};
    EXPECT_TRUE(obj.get("x").parent().is(obj));
}

TEST(Object, TestSimpleSource_Read) {
    auto dsrc = new TestSimpleSource(R"("Strong, black tea")");
    Object obj{dsrc};
    EXPECT_EQ(obj, "Strong, black tea");
    EXPECT_TRUE(dsrc->read_called);
}

TEST(Object, TestSimpleSource_Reset) {
    auto dsrc = new TestSimpleSource(R"("Strong, black tea")");
    Object obj{dsrc};
    EXPECT_EQ(obj, "Strong, black tea");
    EXPECT_TRUE(dsrc->read_called);
    EXPECT_TRUE(dsrc->is_fully_cached());
    dsrc->data = "More strong, black tea";
    obj.reset();
    EXPECT_FALSE(dsrc->is_fully_cached());
    EXPECT_EQ(obj, "More strong, black tea");
    EXPECT_TRUE(dsrc->read_called);
}

TEST(Object, TestSimpleSource_Save) {
    auto dsrc = new TestSimpleSource(R"("Ceylon tea")");
    Object obj{dsrc};
    obj.set("Assam tea");
    EXPECT_FALSE(dsrc->read_meta_called);
    EXPECT_FALSE(dsrc->read_called);
    EXPECT_EQ(obj, "Assam tea");
    EXPECT_FALSE(dsrc->read_called);
    obj.save();
    EXPECT_TRUE(dsrc->write_called);
    obj.reset();
    EXPECT_EQ(obj, "Assam tea");
    EXPECT_TRUE(dsrc->read_called);
}

TEST(Object, TestSimpleSource_SaveNoChange) {
    auto dsrc = new TestSimpleSource(R"("Ceylon tea")");
    Object obj{dsrc};
    EXPECT_EQ(obj, "Ceylon tea");
    EXPECT_TRUE(dsrc->read_called);
    obj.save();
    EXPECT_FALSE(dsrc->write_called);
}

TEST(Object, TestSimpleSource_SaveNoChangeSave) {
    auto dsrc = new TestSimpleSource(R"("Ceylon tea")");
    Object obj{dsrc};
    EXPECT_EQ(obj, "Ceylon tea");
    EXPECT_TRUE(dsrc->read_called);
    obj.save();
    EXPECT_FALSE(dsrc->write_called);
    obj.set("Assam tea");
    EXPECT_FALSE(dsrc->write_called);
    obj.save();
    EXPECT_TRUE(dsrc->write_called);
    obj.reset();
    EXPECT_EQ(obj, "Assam tea");
    EXPECT_TRUE(dsrc->read_called);
}

TEST(Object, TestSimpleSource_DeleteAndSave) {
    auto dsrc = new TestSimpleSource("{'tea': 'Ceylon tea'}");
    Object obj{dsrc};
    EXPECT_EQ(obj.get("tea"), "Ceylon tea");
    EXPECT_TRUE(dsrc->read_called);
    obj.del("tea");
    obj.save();
    EXPECT_TRUE(obj.get("tea").is_null());
    EXPECT_TRUE(dsrc->write_called);
}

TEST(Object, TestSparseSource_KeyIterator) {
    auto dsrc = new TestSparseSource(R"({"x": 1, "y": 2})");
    Object obj{dsrc};

    EXPECT_FALSE(dsrc->iter_deleted);
    KeyList found;
    for (auto& key : obj.iter_keys())
        found.push_back(key);
    EXPECT_TRUE(dsrc->iter_deleted);
    ASSERT_EQ(found.size(), 2);
    EXPECT_EQ(found[0], "x");
    EXPECT_EQ(found[1], "y");
}

TEST(Object, TestSparseSource_ValueIterator) {
    auto dsrc = new TestSparseSource(R"({"x": "X", "y": "Y"})");
    Object obj{dsrc};

    EXPECT_FALSE(dsrc->iter_deleted);
    List found;
    for (auto& value : obj.iter_values())
        found.push_back(value);
    EXPECT_TRUE(dsrc->iter_deleted);
    ASSERT_EQ(found.size(), 2);
    EXPECT_EQ(found[0], "X");
    EXPECT_EQ(found[1], "Y");
}

TEST(Object, TestSparseSource_ItemIterator) {
    auto dsrc = new TestSparseSource(R"({"x": "X", "y": "Y"})");
    Object obj{dsrc};

    EXPECT_FALSE(dsrc->iter_deleted);
    ItemList found;
    for (auto& item : obj.iter_items())
        found.push_back(item);
    EXPECT_TRUE(dsrc->iter_deleted);
    ASSERT_EQ(found.size(), 2);
    EXPECT_EQ(found[0], std::make_pair(Key{"x"}, Object{"X"}));
    EXPECT_EQ(found[1], std::make_pair(Key{"y"}, Object{"Y"}));
}

TEST(Object, TestSparseSource_ReadKey) {
    auto dsrc = new TestSparseSource(R"({"x": 1, "y": 2})");
    Object obj{dsrc};
    EXPECT_EQ(obj.get("x"), 1);
    EXPECT_TRUE(dsrc->read_key_called);
    dsrc->read_key_called = false;
    EXPECT_EQ(obj.get("y"), 2);
    EXPECT_TRUE(dsrc->read_key_called);
}

TEST(Object, TestSparseSource_WriteKey) {
    auto dsrc = new TestSparseSource(R"({"x": 1, "y": 2, "z": 3})");
    Object obj{dsrc};
    obj.set("x", 9);
    obj.set("z", 10);
    EXPECT_FALSE(dsrc->write_called);
    EXPECT_FALSE(dsrc->write_key_called);
    EXPECT_EQ(dsrc->m_cache.get("x"), 9);
    EXPECT_EQ(dsrc->data.get("x"), 1);
    EXPECT_EQ(dsrc->m_cache.get("z"), 10);
    EXPECT_EQ(dsrc->data.get("z"), 3);
    obj.save();
    EXPECT_TRUE(dsrc->write_key_called);
    EXPECT_FALSE(dsrc->write_called);
    EXPECT_EQ(dsrc->m_cache.get("x"), 9);
    EXPECT_EQ(dsrc->data.get("x"), 9);
    EXPECT_EQ(dsrc->m_cache.get("z"), 10);
    EXPECT_EQ(dsrc->data.get("z"), 10);
}

TEST(Object, TestSparseSource_Write) {
    auto dsrc = new TestSparseSource(R"({"x": 1, "y": 2})");
    Object obj{dsrc};
    obj.set(parse_json(R"({"x": 9, "y": 10})"));
    EXPECT_FALSE(dsrc->write_called);
    EXPECT_FALSE(dsrc->write_key_called);
    EXPECT_EQ(dsrc->m_cache.get("x"), 9);
    EXPECT_EQ(dsrc->data.get("x"), 1);
    obj.save();
    EXPECT_TRUE(dsrc->write_called);
    EXPECT_FALSE(dsrc->write_key_called);
    EXPECT_EQ(dsrc->m_cache.get("x"), 9);
    EXPECT_EQ(dsrc->data.get("x"), 9);
}

//TEST(Object, StringSource_Read) {
//    auto dsrc = new ShallowSource(R"("Strong, black tea")");
//    Object obj{dsrc};
//    EXPECT_EQ(obj, "Strong, black tea");
//    EXPECT_FALSE(dsrc->read_called);
//}
//
//TEST(Object, StringSource_ReadKeyThrows) {
//    try {
//        auto dsrc = new ShallowSource(R"("Oops!")");
//        Object obj{dsrc};
//        obj["x"];
//        FAIL();
//    } catch (...) {
//    }
//}
//
//TEST(Object, StringSource_Write) {
//    auto dsrc = new ShallowSource(R"("Ceylon Tea")");
//    Object obj{dsrc};
//    obj = "Assam Tea";
//    EXPECT_EQ(dsrc->data, "Assam Tea");
//    EXPECT_TRUE(dsrc->cached.is_empty());
//    EXPECT_EQ(obj, "Assam Tea");
//}
//
//TEST(Object, StringSource_WriteMemoryBypass) {
//    auto dsrc_1 = new ShallowSource(R"("Ceylon Tea")");
//    auto dsrc_2 = new ShallowSource(R"("Assam Tea")");
//    Object obj_1{dsrc_1};
//    Object obj_2{dsrc_2};
//    obj_1 = obj_2;
//    EXPECT_TRUE(dsrc_1->memory_bypass);
//    EXPECT_TRUE(dsrc_2->cached.is_empty());
//    EXPECT_EQ(obj_1, "Assam Tea");
//    EXPECT_EQ(obj_2, "Assam Tea");
//}
//
//TEST(Object, StringSource_IterateString) {
//    auto dsrc = new ShallowSource(R"("0123456,789AB")");
//    Object obj{dsrc};
//    std::string got;
//    obj.iter_visit(overloaded {
//        [&got] (const char c) -> bool {
//            if (c != ',') { got.push_back(c); return true; }
//            return false;
//        },
//        [] (const Object&) -> bool { return true; },
//        [] (const Key&) -> bool { return true; }
//    });
//    EXPECT_EQ(got, "0123456");
//    EXPECT_FALSE(dsrc->read_called);
//    EXPECT_FALSE(dsrc->read_key_called);
//}
