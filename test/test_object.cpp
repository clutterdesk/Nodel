#include <gtest/gtest.h>
#include <nodel/nodel.h>
#include <fmt/core.h>

using namespace nodel;

Object from_json(const std::string& json) {
    std::optional<nodel::json::ParseError> parse_error;
    Object result = nodel::json::parse(json, parse_error);
    if (parse_error) {
        throw JsonException(parse_error->to_str());
        return {};
    }
    return result;
}

TEST(Object, Null) {
  Object v;
  EXPECT_TRUE(v.is_null());
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
  List base_list{Object(1), Object("tea"), Object(3.14), Object(true)};
  Object list{base_list};
  EXPECT_TRUE(list.is_list());
  EXPECT_EQ(list.to_json(), "[1, \"tea\", 3.14, true]");
}

TEST(Object, Map) {
  Map base_map{};
  Object map{base_map};
  EXPECT_TRUE(map.is_map());
}

TEST(Object, MapKeyOrder) {
  Map base_map{{"x", Object(100)}, {"y", Object("tea")}, {90, Object(true)}};
  Object map{base_map};
  EXPECT_TRUE(map.is_map());
  EXPECT_EQ(map.to_json(), "{\"x\": 100, \"y\": \"tea\", 90: true}");
}

TEST(Object, ToStr) {
  EXPECT_EQ(Object{}.to_str(), "null");
  EXPECT_EQ(Object{false}.to_str(), "0");
  EXPECT_EQ(Object{true}.to_str(), "1");
  EXPECT_EQ(Object{7LL}.to_str(), "7");
  EXPECT_EQ(Object{0xFFFFFFFFFFFFFFFFULL}.to_str(), "18446744073709551615");
  EXPECT_EQ(Object{3.14}.to_str(), "3.14");
  EXPECT_EQ(Object{"trivial"}.to_str(), "trivial");
  EXPECT_EQ(from_json("[1, 2, 3]").to_str(), "[1, 2, 3]");
  EXPECT_EQ(from_json("{'name': 'Dude'}").to_str(), "{\"name\": \"Dude\"}");

  const char* json = R"({"a": [], "b": [1], "c": [2, 3], "d": [4, [5, 6]]})";
  EXPECT_EQ(from_json(json).to_str(), json);
}

TEST(Object, CompareBoolBool) {
  Object a{true};
  Object b{true};
  EXPECT_FALSE(a != b);
  EXPECT_TRUE(a == b);
  EXPECT_FALSE(a < b);
  EXPECT_FALSE(a > b);
}

TEST(Object, CompareBoolInt) {
  Object a{true};
  Object b{1};
  EXPECT_FALSE(a != b);
  EXPECT_TRUE(a == b);
  EXPECT_FALSE(a < b);
  EXPECT_FALSE(a > b);
}

TEST(Object, CompareBoolFloat) {
  Object a{true};
  Object b{1.0};
  EXPECT_FALSE(a != b);
  EXPECT_TRUE(a == b);
  EXPECT_FALSE(a < b);
  EXPECT_FALSE(a > b);
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
}

TEST(Object, CompareIntFloat) {
  Object a{1};
  Object b{1.0};
  EXPECT_TRUE(a == b);
  EXPECT_FALSE(a < b);
  EXPECT_FALSE(a > b);
}

TEST(Object, CompareUIntFloat) {
  Object a{0xFFFFFFFFFFFFFFFFULL};
  Object b{1e100};
  EXPECT_TRUE(a != b);
  EXPECT_TRUE(a < b);
  EXPECT_FALSE(a > b);
}

TEST(Object, ToKey) {
  Object obj{"key"};
  EXPECT_TRUE(obj.is_str());
  EXPECT_EQ(obj.to_key().as_str(), "key");
}

TEST(Object, SubscriptList) {
  Object obj = from_json("[7, 8, 9]");
  EXPECT_TRUE(obj.is_list());
  EXPECT_EQ(obj[0].to_int(), 7);
  EXPECT_EQ(obj[1].to_int(), 8);
  EXPECT_EQ(obj[2].to_int(), 9);
  EXPECT_EQ(obj[Object{0}].to_int(), 7);
  EXPECT_EQ(obj[Object{1}].to_int(), 8);
  EXPECT_EQ(obj[Object{2}].to_int(), 9);
}

TEST(Object, SubscriptMap) {
  Object obj = from_json(R"({0: 7, 1: 8, 2: 9, "name": "Brian"})");
  EXPECT_TRUE(obj.is_map());
  EXPECT_EQ(obj[0].to_int(), 7);
  EXPECT_EQ(obj[1].to_int(), 8);
  EXPECT_EQ(obj[2].to_int(), 9);
  EXPECT_EQ(obj["name"].as_str(), "Brian");
  EXPECT_EQ(obj[Key{0}].to_int(), 7);
  EXPECT_EQ(obj[Key{1}].to_int(), 8);
  EXPECT_EQ(obj[Key{2}].to_int(), 9);
  EXPECT_EQ(obj[Object{0}].to_int(), 7);
  EXPECT_EQ(obj[Object{1}].to_int(), 8);
  EXPECT_EQ(obj[Object{2}].to_int(), 9);
}

TEST(Object, MultipleSubscriptMap) {
  Object obj = from_json(R"({"a": {"b": {"c": 7}}})");
  EXPECT_TRUE(obj.is_map());
  EXPECT_EQ(obj.get("a").get("b").get("c"), 7);
}

TEST(Object, WalkDF) {
  Object obj = from_json("[1, [2, [3, [4, 5], 6], 7], 8]");
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
  Object obj = from_json("[1, [2, [3, [4, 5], 6], 7], 8]");
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

TEST(Object, RefCountPrimitive) {
  Object obj{7};
  EXPECT_TRUE(obj.is_int());
  EXPECT_EQ(obj.ref_count(), std::numeric_limits<size_t>::max());
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
  EXPECT_TRUE(obj.is_null());
  EXPECT_TRUE(copy.is_str());
  EXPECT_EQ(obj.ref_count(), std::numeric_limits<size_t>::max());
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
  EXPECT_TRUE(obj.is_null());
  EXPECT_TRUE(copy.is_str());
  EXPECT_EQ(obj.ref_count(), std::numeric_limits<size_t>::max());
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
  Object obj{};
  Object num{0};
  EXPECT_NE(obj.id(), num.id());
}

// Omitted on purpose.  See implementation.
//TEST(Object, CustomHash) {
//  Object obj{"assam tea"};
//  EXPECT_EQ(obj.hash(), std::hash<std::string>{}("assam tea"));
//  EXPECT_EQ(std::hash<Object>{}(obj), std::hash<std::string>{}("assam tea"));
//}
