#include <gtest/gtest.h>
#include <nodel/nodel.h>
#include <fmt/core.h>
#include <algorithm>

#include <nodel/impl/json.h>

using namespace nodel;

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
  EXPECT_EQ(map.to_json(), "{\"x\": 100, \"y\": \"tea\", 90: true}");
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
  EXPECT_EQ(obj.to_key().as_str(), "key");
}

TEST(Object, SubscriptList) {
  Object obj = parse_json("[7, 8, 9]");
  EXPECT_TRUE(obj.is_list());
  EXPECT_EQ(obj[0].to_int(), 7);
  EXPECT_EQ(obj[1].to_int(), 8);
  EXPECT_EQ(obj[2].to_int(), 9);
  EXPECT_EQ(obj[Object{0}].to_int(), 7);
  EXPECT_EQ(obj[Object{1}].to_int(), 8);
  EXPECT_EQ(obj[Object{2}].to_int(), 9);
}

TEST(Object, SubscriptMap) {
  Object obj = parse_json(R"({0: 7, 1: 8, 2: 9, "name": "Brian"})");
  EXPECT_TRUE(obj.is_map());
  EXPECT_EQ(obj[0].to_int(), 7);
  EXPECT_EQ(obj[1].to_int(), 8);
  EXPECT_EQ(obj[2].to_int(), 9);
  EXPECT_EQ(obj["name"sv].as<String>(), "Brian");
  EXPECT_EQ(obj[Key{"name"sv}].as<String>(), "Brian");
  EXPECT_EQ(obj["name"].as<String>(), "Brian");
  EXPECT_EQ(obj[Key{"name"}].as<String>(), "Brian");
  EXPECT_EQ(obj["name"s].as<String>(), "Brian");
  EXPECT_EQ(obj[Key{"name"s}].as<String>(), "Brian");
  EXPECT_EQ(obj[Key{0}].to_int(), 7);
  EXPECT_EQ(obj[Key{1}].to_int(), 8);
  EXPECT_EQ(obj[Key{2}].to_int(), 9);
  EXPECT_EQ(obj[Object{0}].to_int(), 7);
  EXPECT_EQ(obj[Object{1}].to_int(), 8);
  EXPECT_EQ(obj[Object{2}].to_int(), 9);
}

TEST(Object, MultipleSubscriptMap) {
  Object obj = parse_json(R"({"a": {"b": {"c": 7}}})");
  EXPECT_TRUE(obj.is_map());
  EXPECT_EQ(obj.get("a").get("b").get("c"), 7);
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
    EXPECT_EQ(Object{null}.key_of(obj), "null");
    EXPECT_EQ(obj.key_of(obj["bool"]), "bool");
    EXPECT_EQ(obj.key_of(obj["int"]), "int");
    EXPECT_EQ(obj.key_of(obj["uint"]), "uint");
    EXPECT_EQ(obj.key_of(obj["float"]), "float");
    EXPECT_EQ(obj.key_of(obj["str"]), "str");
    EXPECT_EQ(obj.key_of(obj["list"]), "list");
    EXPECT_EQ(obj.key_of(obj["map"]), "map");
    EXPECT_EQ(obj.key_of(obj["redun_bool"]), "bool");
    EXPECT_EQ(obj.key_of(obj["redun_int"]), "int");
    EXPECT_EQ(obj.key_of(obj["redun_uint"]), "uint");
    EXPECT_EQ(obj.key_of(obj["redun_float"]), "float");
    EXPECT_EQ(obj.key_of(obj["okay_str"]), "okay_str");
}

TEST(Object, AncestorRange) {
    Object obj = parse_json(R"({"a": {"b": ["Assam", "Ceylon"]}})");
    List ancestors;
    for (auto anc : obj["a"]["b"][1].ancestors())
        ancestors.push_back(anc);
    EXPECT_EQ(ancestors.size(), 3);
    EXPECT_EQ(ancestors[0].id(), obj["a"]["b"].id());
    EXPECT_EQ(ancestors[1].id(), obj["a"].id());
    EXPECT_EQ(ancestors[2].id(), obj.id());
}

TEST(Object, ListChildrenRange) {
    Object obj = parse_json(R"([true, 1, "x"])");
    List children;
    for (auto obj : obj.children())
        children.push_back(obj);
    EXPECT_EQ(children.size(), 3);
    EXPECT_TRUE(children[0].is_bool());
    EXPECT_TRUE(children[1].is_int());
    EXPECT_TRUE(children[2].is_str());
}

TEST(Object, OrderedMapChildrenRange) {
    Object obj = parse_json(R"({"a": true, "b": 1, "c": "x"})");
    List children;
    for (auto obj : obj.children())
        children.push_back(obj);
    EXPECT_EQ(children.size(), 3);
    EXPECT_TRUE(children[0].is_bool());
    EXPECT_TRUE(children[1].is_int());
    EXPECT_TRUE(children[2].is_str());
}

TEST(Object, ListSiblingRange) {
    Object obj = parse_json(R"(["a", "b", "c"])");
    List siblings;
    for (auto obj : obj[1].siblings())
        siblings.push_back(obj);
    EXPECT_EQ(siblings.size(), 2);
    EXPECT_EQ(siblings[0], "a");
    EXPECT_EQ(siblings[1], "c");
}

TEST(Object, MapSiblingRange) {
    Object obj = parse_json(R"({"a": "A", "b": "B", "c": "C"})");
    List siblings;
    for (auto obj : obj["b"].siblings())
        siblings.push_back(obj);
    EXPECT_EQ(siblings.size(), 2);
    EXPECT_EQ(siblings[0], "A");
    EXPECT_EQ(siblings[1], "C");
}

TEST(Object, DescendantRange) {
    Object obj = parse_json(R"({
        "a": {"aa": "AA", "ab": "AB"}, 
        "b": [{"b0a": "B0A", "b0b": "B0B"}, 
              {"b1a": "B1A", "b1b": ["B1B0"], "b1c": {"b1ca": "B1CA"}}, 
              "B2"]
    })");
    List list;
    for (auto des : obj.descendants())
        list.push_back(des);
    EXPECT_EQ(list.size(), 14);
    EXPECT_EQ(list[0].id(), obj["a"].id());
    EXPECT_EQ(list[1].id(), obj["b"].id());
    EXPECT_EQ(list[2].id(), obj["a"]["aa"].id());
    EXPECT_EQ(list[3].id(), obj["a"]["ab"].id());
    EXPECT_EQ(list[4].id(), obj["b"][0].id());
    EXPECT_EQ(list[5].id(), obj["b"][1].id());
    EXPECT_EQ(list[6].id(), obj["b"][2].id());
    EXPECT_EQ(list[7].id(), obj["b"][0]["b0a"].id());
    EXPECT_EQ(list[8].id(), obj["b"][0]["b0b"].id());
    EXPECT_EQ(list[9].id(), obj["b"][1]["b1a"].id());
    EXPECT_EQ(list[10].id(), obj["b"][1]["b1b"].id());
    EXPECT_EQ(list[11].id(), obj["b"][1]["b1c"].id());
    EXPECT_EQ(list[12].id(), obj["b"][1]["b1b"][0].id());
    EXPECT_EQ(list[13].id(), obj["b"][1]["b1c"]["b1ca"].id());
}

TEST(Object, ChildrenRangeMultiuser) {
    Object o1 = parse_json(R"({"a": "A", "b": "B", "c": "C"})");
    Object o2 = parse_json(R"({"x": "X", "y": "Y", "z": "Z"})");
    List result;
    auto r1 = o1.children();
    auto it1 = r1.begin();
    auto end1 = r1.end();
    auto r2 = o2.children();
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
    EXPECT_EQ(obj["a"].path().to_str(), ".a");
    EXPECT_EQ(obj["a"]["b"].path().to_str(), ".a.b");
    EXPECT_EQ(obj["a"]["b"][1].path().to_str(), ".a.b[1]");
    EXPECT_EQ(obj["a"]["b"][0].path().to_str(), ".a.b[0]");
    Path path = obj["a"]["b"][1].path();
    EXPECT_EQ(obj.get(path).id(), obj["a"]["b"][1].id());
}

//TEST(Object, ParsePath) {
//    Path path{"a.b[1].c[2][3].d"};
//    KeyList keys = {"a", "b", 1, "c", 2, 3, "d"};
//    EXPECT_EQ(path.keys(), keys);
//}

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
    EXPECT_TRUE(obj["x"].is_list());
    EXPECT_EQ(obj["x"][0], 1);
    // ref count error might not manifest with one try
    for (int i=0; i<100; i++) {
        obj["x"] = obj["x"];
        EXPECT_TRUE(obj["x"].is_list());
        ASSERT_EQ(obj["x"][0], 1);
    }
}

TEST(Object, RootParentIsEmpty) {
    Object root = parse_json(R"({"x": [1], "y": [2]})");
    EXPECT_TRUE(root.parent().is_null());
}

TEST(Object, ClearParentOnOverwrite) {
    List list;
    for (int i=0; i<100000; i++)
        list.push_back(Object(std::to_string(i)));

    Object root{std::move(list)};
    for (auto& obj : list)
        EXPECT_TRUE(obj.parent().is(root));

    root = null;

    for (auto& obj : list)
        EXPECT_TRUE(obj.parent().is_null());
}

//TEST(Object, ParentIntegrityOnRemove) {
//    Object par = parse_json(R"({"x": [1], "y": [2]})");
//    Object x1 = par["x"];
//    Object x2 = par["x"];
//    EXPECT_TRUE(x1.parent().is_map());
//    EXPECT_EQ(x2.parent().id(), par.id());
//    EXPECT_TRUE(x1.parent().is_map());
//    EXPECT_EQ(x2.parent().id(), par.id());
//    par.remove("x");
//    EXPECT_TRUE(x1.parent().is_null());
//    EXPECT_TRUE(x2.parent().is_null());
//}

TEST(Object, GetKeys) {
    Object obj = parse_json(R"({"x": [1], "y": [2]})");
    KeyList expect = {"x", "y"};
    EXPECT_EQ(obj.keys(), expect);
}

TEST(Object, IterString) {
    Object obj{"01234567"};
    std::string got;
    obj.iter_visit(overloaded {
        [&got] (const char c) -> bool {
            got.push_back(c);
            return c != '4';
        },
        [] (const Object&) -> bool { return true; },
        [] (const Key&) -> bool { return true; }
    });
    EXPECT_EQ(got, "01234");
}


struct TestObjectSource : public ObjectDataSource
{
    TestObjectSource(const std::string& json) {
        data = parse_json(json);
    }

    ~TestObjectSource() override {}

    void read_meta(Object& cache) override {
        std::istringstream in{data.to_json()};
        json::impl::Parser parser{in};
        cache = Object{(Object::ReprType)parser.parse_type()};
    }

    void read(Object& cache) override {
        read_called = true;
        cache = data;
    }

    void write(const Object& obj) override {
        memory_bypass = false;
        if (obj.has_data_source()) {  // for test purposes, assume same data-source
            // cache bypass
            memory_bypass = true;
            auto& dsrc = obj.data_source<TestObjectSource>();
            data.release();
            data = dsrc.data;
        } else {
            data.release();
            data = obj;
        }
    }

    void write(Object&& obj) override {
        memory_bypass = false;
        if (obj.has_data_source()) {  // for test purposes, assume same data-source
            // cache bypass
            memory_bypass = true;
            auto& dsrc = obj.data_source<TestObjectSource>();
            data.release();
            data = dsrc.data;
        } else {
            data.release();
            data = std::forward<Object>(obj);
        }
    }

    struct Iterator : public IDataSourceIterator
    {
        size_t chunk_size = 3;

        Iterator(Object data) : data{data}, sc{nullptr} {}

        ~Iterator() {
            if (sc != nullptr) {
                switch (data.type()) {
                    case Object::STR_I:  delete sc; break;
                    case Object::LIST_I: delete lc; break;
                    case Object::OMAP_I: delete klc; break;
                    default: break;  // undefined behavior
                }
            }
        }

        std::string* iter_begin_str() override {
            sc = new std::string{};
            sc->reserve(chunk_size);
            return sc;
        }

        List* iter_begin_list() override {
            lc = new List{};
            lc->reserve(chunk_size);
            return lc;
        }

        KeyList* iter_begin_key_list() override {
            klc = new KeyList{};
            klc->reserve(chunk_size);
            return klc;
        }

        size_t iter_next() override {
            switch (data.type()) {
                case Object::STR_I: {
                    auto& chunk = *sc;
                    const auto& str = data.as<String>();
                    chunk.resize(std::min(chunk_size, str.size() - pos));
                    memcpy(chunk.data(), data.as<String>().data() + pos, chunk.size());
                    pos += chunk.size();
                    return chunk.size();
                }
                case Object::LIST_I: {
                    auto& chunk = *lc;
                    auto child_it = data.children().begin();
                    chunk.resize(std::min(chunk.size(), data.size() - pos));
                    for (size_t i=0; i < chunk.size(); ++i, ++child_it) {
                        chunk[i] = *child_it;
                    }
                    pos += chunk.size();
                    return chunk.size();
                }
                case Object::OMAP_I: {
                    auto& chunk = *klc;
                    auto keys = data.keys();
                    chunk.resize(std::min(chunk.size(), keys.size() - pos));
                    for (size_t i=0; i < chunk.size(); i++) {
                        chunk[i] = keys[pos + i];
                    }
                    pos += chunk.size();
                    return chunk.size();
                }
                default:
                    throw WrongType(Object::type_name(data.type()));
            }
        }

        void iter_end() override {}

        Object data;
        int pos = 0;

        union {
            std::string* sc;
            List* lc;
            KeyList* klc;
        };
    };

    IDataSourceIterator* new_iter() const override {
        return new Iterator{data};
    }

    Object data;
    bool read_called = false;
    bool memory_bypass = false;
};

struct TestKeySource : public KeyDataSource
{
    TestKeySource(const std::string& json) {
        cache = Map{};
        data = parse_json(json);
    }

    ~TestKeySource() override {}

    void read_meta(Object& cache) override {
        std::istringstream in{data.to_json()};
        json::impl::Parser parser{in};
        cache = Object{(Object::ReprType)parser.parse_type()};
    }

    Object read_key(const Key& key) override {
        read_key_called = true;
        cache.set(key, data.get(key));
        return cache.get(key);
    }

    void write_key(const Key& key, const Object& obj) override {
    }

    void write_key(const Key& key, Object&& obj) override {
    }

    size_t size() override {
        return data.size();
    }

    KeyList keys() override {
        return data.keys();
    }

    struct Iterator : public IDataSourceIterator
    {
        size_t chunk_size = 3;

        Iterator(Object data) : data{data}, sc{nullptr} {}

        ~Iterator() {
            if (sc != nullptr) {
                switch (data.type()) {
                    case Object::STR_I:  delete sc; break;
                    case Object::LIST_I: delete lc; break;
                    case Object::OMAP_I: delete klc; break;
                    default: break;  // undefined behavior
                }
            }
        }

        KeyList* iter_begin_key_list() override {
            klc = new KeyList{};
            klc->reserve(chunk_size);
            return klc;
        }

        size_t iter_next() override {
        }

        void iter_end() override {}

        Object data;
        int pos = 0;

        union {
            std::string* sc;
            List* lc;
            KeyList* klc;
        };
    };

    IDataSourceIterator* new_iter() const override {
        return new Iterator{data};
    }

    Object data;
    Object::ReprType cached_type = Object::BAD_I;
    bool read_key_called = false;
    bool memory_bypass = false;
};

TEST(Object, TestObjectSource_RefCountCopyAssign) {
    Object obj{new TestObjectSource(R"("foo")")};
    EXPECT_EQ(obj.ref_count(), 1);
    Object copy = obj;
    EXPECT_EQ(obj.ref_count(), 2);
    EXPECT_EQ(copy.ref_count(), 2);
    obj.release();
    EXPECT_EQ(copy.ref_count(), 1);
}

TEST(Object, ShallowTestObjectSource_RefCountMoveAssign) {
    Object obj{new TestObjectSource(R"("foo")")};
    EXPECT_EQ(obj.ref_count(), 1);
    Object copy = std::move(obj);
    EXPECT_EQ(copy.ref_count(), 1);
    obj.release();
    EXPECT_EQ(copy.ref_count(), 1);
}

TEST(Object, TestObjectSource_GetType) {
    Object obj{new TestObjectSource(R"({"x": 1, "y": 2})")};
    EXPECT_TRUE(obj.is_map());
}

TEST(Object, TestObjectSource_Read) {
    auto dsrc = new TestObjectSource(R"("Strong, black tea")");
    Object obj{dsrc};
    EXPECT_EQ(obj, "Strong, black tea");
    EXPECT_TRUE(dsrc->read_called);
}

TEST(Object, TestKeySource_ReadKey) {
    auto dsrc = new TestKeySource(R"({"x": 1, "y": 2})");
    Object obj{dsrc};
    EXPECT_EQ(obj["x"], 1);
    EXPECT_TRUE(dsrc->read_key_called);
    dsrc->read_key_called = false;
    EXPECT_EQ(obj["y"], 2);
    EXPECT_TRUE(dsrc->read_key_called);
}

//TEST(Object, ShallowSource_Write) {
//    auto dsrc = new ShallowSource(R"({"x": 1, "y": 2})");
//    Object obj{dsrc};
//    obj = parse_json(R"({"x": 9, "y": 10})");
//    EXPECT_EQ(dsrc->data["x"], 9);
//    EXPECT_TRUE(dsrc->cached.is_empty());
//    EXPECT_EQ(obj["x"], 9);
//    EXPECT_TRUE(dsrc->read_key_called);
//    dsrc->read_key_called = false;
//    EXPECT_EQ(obj["y"], 10);
//    EXPECT_TRUE(dsrc->read_key_called);
//}
//
//TEST(Object, ShallowSource_WriteMemoryBypass) {
//    auto dsrc_1 = new ShallowSource(R"({"x": 1, "y": 2})");
//    auto dsrc_2 = new ShallowSource(R"({"x": 3, "y": 4})");
//    Object obj_1{dsrc_1};
//    Object obj_2{dsrc_2};
//    obj_1 = obj_2;
//    EXPECT_TRUE(dsrc_1->memory_bypass);
//    EXPECT_TRUE(dsrc_2->cached.is_empty());
//    EXPECT_EQ(obj_1["x"], 3);
//    EXPECT_FALSE(dsrc_2->read_key_called);
//    EXPECT_EQ(obj_2["x"], 3);
//    EXPECT_TRUE(dsrc_2->read_key_called);
//}
//
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

TEST(Object, StringSubscriptDangling) {
  Object obj = parse_json(R"({"tea": "FTGFOP"})");
  EXPECT_EQ(obj["tea"].as<String>(), "FTGFOP");
  EXPECT_EQ(obj["tea"].as<String>(), "FTGFOP");
  EXPECT_EQ(obj["tea"].as<String>(), "FTGFOP");
  EXPECT_EQ(obj["tea"].as<String>(), "FTGFOP");
  EXPECT_EQ(obj["tea"].as<String>(), "FTGFOP");
  EXPECT_EQ(obj["tea"].as<String>(), "FTGFOP");
  EXPECT_EQ(obj["tea"].as<String>(), "FTGFOP");
  EXPECT_EQ(obj["tea"].as<String>(), "FTGFOP");
  EXPECT_EQ(obj["tea"].as<String>(), "FTGFOP");
  EXPECT_EQ(obj["tea"].as<String>(), "FTGFOP");
  EXPECT_EQ(obj["tea"].as<String>(), "FTGFOP");
  EXPECT_EQ(obj["tea"].as<String>(), "FTGFOP");
  EXPECT_EQ(obj["tea"].as<String>(), "FTGFOP");
  EXPECT_EQ(obj["tea"].as<String>(), "FTGFOP");
  EXPECT_EQ(obj["tea"].as<String>(), "FTGFOP");
  EXPECT_EQ(obj["tea"].as<String>(), "FTGFOP");
}

