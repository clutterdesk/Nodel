/// @file
/// @b License: @n Apache License v2.0
/// @copyright Robert Dunnagan
#include <gtest/gtest.h>
#include <fmt/format.h>
#include <algorithm>

#include <nodel/core/Object.hxx>
#include <nodel/core.hxx>
#include <nodel/parser/json.hxx>

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

class TestObjectAlien : public Alien
{
  public:
    ~TestObjectAlien() override {}
    std::unique_ptr<Alien> clone() override { return std::make_unique<TestObjectAlien>(); }
    String to_str() const override { return "TestObjectAlien"; }
    String to_json(int indent) const override { return "'TestObjectAlien'"; }
};


TEST(Object, TypeName) {
    EXPECT_EQ(Object{}.type_name(), "empty");
    EXPECT_EQ(Object{nil}.type_name(), "nil");
    EXPECT_EQ(Object{true}.type_name(), "bool");
    EXPECT_EQ(Object{-1}.type_name(), "int");
    EXPECT_EQ(Object{1UL}.type_name(), "uint");
    EXPECT_EQ(Object{"foo"}.type_name(), "string");
    EXPECT_EQ(Object{Object::LIST}.type_name(), "list");
    EXPECT_EQ(Object{Object::SMAP}.type_name(), "sorted-map");
    EXPECT_EQ(Object{Object::OMAP}.type_name(), "ordered-map");
}

TEST(Object, Empty) {
  Object v;
  EXPECT_TRUE(v.is_empty());
}

TEST(Object, Null) {
  Object v{nil};
  EXPECT_TRUE(v == nil);
  EXPECT_TRUE(v.is(nil));
  EXPECT_EQ(v.to_json(), "nil");
}

TEST(Object, Bool) {
  Object v{true};
  EXPECT_TRUE(v.is_type<bool>());
  EXPECT_FALSE(nodel::is_number(v));
  EXPECT_EQ(v.to_json(), "true");

  v = false;
  EXPECT_TRUE(v.is_type<bool>());
  EXPECT_EQ(v.to_json(), "false");

  v = Object::BOOL;
  EXPECT_TRUE(v.is_type<bool>());
  EXPECT_EQ(v.to_json(), "false");
}

TEST(Object, Int64) {
  Object v{-0x7FFFFFFFFFFFFFFFLL};
  EXPECT_TRUE(v.is_type<Int>());
  EXPECT_TRUE(nodel::is_number(v));
  EXPECT_EQ(v.to_json(), "-9223372036854775807");

  v = Object::INT;
  EXPECT_TRUE(v.is_type<Int>());
  EXPECT_EQ(v.to_json(), "0");
}

TEST(Object, UInt64) {
  Object v{0xFFFFFFFFFFFFFFFFULL};
  EXPECT_TRUE(v.is_type<UInt>());
  EXPECT_TRUE(nodel::is_number(v));
  EXPECT_EQ(v.to_json(), "18446744073709551615");

  v = Object::UINT;
  EXPECT_TRUE(v.is_type<UInt>());
  EXPECT_EQ(v.to_json(), "0");
}

TEST(Object, Double) {
  Object v{3.141593};
  EXPECT_TRUE(v.is_type<Float>());
  EXPECT_TRUE(nodel::is_number(v));
  EXPECT_EQ(v.to_json(), "3.141593");

  v = Object::FLOAT;
  EXPECT_TRUE(v.is_type<Float>());
  EXPECT_EQ(v.to_json(), "0");
}

TEST(Object, String) {
  Object v{"123"};
  EXPECT_TRUE(v.is_type<String>());
  EXPECT_TRUE(v.parent() == nil);
  EXPECT_EQ(v.to_json(), "\"123\"");

  Object quoted{"a\"b"};
  EXPECT_TRUE(quoted.is_type<String>());
  EXPECT_EQ(quoted.to_json(), "\"a\\\"b\"");
}

TEST(Object, ConstructWithInvalidRepr) {
    try {
        Object v{Object::DSRC};
        FAIL();
    } catch(...) {
    }
}

TEST(Object, ObjectList) {
  Object list{ObjectList{Object(1), Object("tea"), Object(3.14), Object(true)}};
  EXPECT_TRUE(list.is_type<ObjectList>());
  EXPECT_EQ(list.to_json(), "[1, \"tea\", 3.14, true]");
}

TEST(Object, SortedMap) {
  Object map{SortedMap{}};
  EXPECT_TRUE(map.is_type<SortedMap>());
}

TEST(Object, SortedMapKeyOrder) {
  Object map{SortedMap{{"y"_key, Object("tea")}, {"x"_key, Object(100)}, {90, Object(true)}}};
  EXPECT_TRUE(map.is_type<SortedMap>());
  map.to_json();
  EXPECT_EQ(map.to_json(), "{90: true, \"x\": 100, \"y\": \"tea\"}");
}

TEST(Object, OrderedMap) {
  Object map{OrderedMap{}};
  EXPECT_TRUE(map.is_type<OrderedMap>());
}

TEST(Object, OrderedMapKeyOrder) {
  Object map{OrderedMap{{"x"_key, Object(100)}, {"y"_key, Object("tea")}, {90, Object(true)}}};
  EXPECT_TRUE(map.is_type<OrderedMap>());
  map.to_json();
  EXPECT_EQ(map.to_json(), "{\"x\": 100, \"y\": \"tea\", 90: true}");
}

TEST(Object, Size) {
    EXPECT_EQ(Object(nil).size(), 0UL);
    EXPECT_EQ(Object(1LL).size(), 0UL);
    EXPECT_EQ(Object(1ULL).size(), 0UL);
    EXPECT_EQ(Object(1.0).size(), 0UL);
    EXPECT_EQ(Object("foo").size(), 3UL);
    EXPECT_EQ("[1, 2, 3]"_json.size(), 3UL);
    EXPECT_EQ("{'x': 1, 'y': 2}"_json.size(), 2UL);
    json::Options options; options.use_sorted_map = true;
    EXPECT_EQ(json::parse(options, "{'x': 1, 'y': 2}").size(), 2UL);
}

TEST(Object, ToBool) {
    EXPECT_EQ(Object{nil}.cast<bool>(), false);
    EXPECT_EQ(Object{true}.cast<bool>(), true);
    EXPECT_EQ(Object{0}.cast<bool>(), false);
    EXPECT_EQ(Object{1}.cast<bool>(), true);
    EXPECT_EQ(Object{0UL}.cast<bool>(), false);
    EXPECT_EQ(Object{1UL}.cast<bool>(), true);
    EXPECT_EQ(Object{0.0}.cast<bool>(), false);
    EXPECT_EQ(Object{1.0}.cast<bool>(), true);
    EXPECT_EQ(Object{"false"}.cast<bool>(), false);
    EXPECT_EQ(Object{"true"}.cast<bool>(), true);

    try {
        Object obj;
        obj.cast<bool>();
        FAIL();
    } catch (...) {
    }
}

TEST(Object, ToInt) {
    EXPECT_EQ(Object{nil}.cast<Int>(), 0);
    EXPECT_EQ(Object{false}.cast<Int>(), 0);
    EXPECT_EQ(Object{true}.cast<Int>(), 1);
    EXPECT_EQ(Object{-1}.cast<Int>(), -1);
    EXPECT_EQ(Object{1UL}.cast<Int>(), 1);
    EXPECT_EQ(Object{3.0}.cast<Int>(), 3);
    EXPECT_EQ(Object{"-1"}.cast<Int>(), -1);

    try {
        Object obj;
        obj.cast<Int>();
        FAIL();
    } catch (...) {
    }
}

TEST(Object, ToUInt) {
    EXPECT_EQ(Object{nil}.cast<UInt>(), 0ULL);
    EXPECT_EQ(Object{false}.cast<UInt>(), 0UL);
    EXPECT_EQ(Object{true}.cast<UInt>(), 1UL);
    EXPECT_EQ(Object{-1}.cast<UInt>(), (UInt)(-1));
    EXPECT_EQ(Object{(UInt)(-1)}.cast<UInt>(), (UInt)(-1));
    EXPECT_EQ(Object{"3"}.cast<UInt>(), 3UL);

// NOTE: crashes with -On for n > 0 with EXC_BAD_INSTRUCTION (code=EXC_I386_INVOP, subcode=0x0)
//    Float minus_one = -1.0;
//    EXPECT_EQ(Object{-1.0}.cast<UInt>(), (UInt)minus_one);

    try {
        Object obj;
        obj.cast<UInt>();
        FAIL();
    } catch (...) {
    }
}

TEST(Object, ToFloat) {
    EXPECT_EQ(Object{nil}.cast<Float>(), 0.0);
    EXPECT_EQ(Object{false}.cast<Float>(), (Float)(false));
    EXPECT_EQ(Object{true}.cast<Float>(), (Float)(true));
    EXPECT_EQ(Object{-1}.cast<Float>(), (Float)(-1));
    EXPECT_EQ(Object{(UInt)(-1)}.cast<Float>(), (Float)((UInt)(-1)));
    EXPECT_EQ(Object{0.33333333}.cast<Float>(), 0.33333333);
    EXPECT_EQ(Object{"3.14159"}.cast<Float>(), 3.14159);

    try {
        Object obj;
        obj.cast<Float>();
        FAIL();
    } catch (...) {
    }
}

TEST(Object, StdStreamOperator) {
    Object obj{2.718};
    std::stringstream ss;
    ss << obj;
    EXPECT_EQ(ss.str(), "2.718");
}

TEST(Object, ToStr) {
  EXPECT_EQ(Object{nil}.to_str(), "nil");
  EXPECT_EQ(Object{false}.to_str(), "false");
  EXPECT_EQ(Object{true}.to_str(), "true");
  EXPECT_EQ(Object{7LL}.to_str(), "7");
  EXPECT_EQ(Object{0xFFFFFFFFFFFFFFFFULL}.to_str(), "18446744073709551615");
  EXPECT_EQ(Object{3.14}.to_str(), "3.14");
  EXPECT_EQ(Object{"trivial"}.to_str(), "trivial");
  EXPECT_EQ("[1, 2, 3]"_json.to_str(), "[1, 2, 3]");
  EXPECT_EQ("{'name': 'Dude'}"_json.to_str(), "{\"name\": \"Dude\"}");

  const char* json = R"({"a": [], "b": [1], "c": [2, 3], "d": [4, [5, 6]]})";
  EXPECT_EQ(json::parse(json).to_str(), json);

  try {
      Object obj;
      obj.to_str();
      FAIL();
  } catch (...) {
  }
}

TEST(Object, ToKey) {
    Object obj{"key"};
    EXPECT_TRUE(obj.is_type<String>());
    EXPECT_EQ(obj.to_key().as<StringView>(), "key");

    EXPECT_EQ(Object{nil}.to_key(), Key{nil});
    EXPECT_EQ(Object{false}.to_key(), Key{false});
    EXPECT_EQ(Object{true}.to_key(), Key{true});
    EXPECT_EQ(Object{-1}.to_key(), Key{-1});
    EXPECT_EQ(Object{1UL}.to_key(), Key{1UL});
    EXPECT_EQ(Object{"tea"}.to_key(), "tea"_key);

    try {
        Object obj;
        obj.to_key();
        FAIL();
    } catch (...) {
    }
}

TEST(Object, NonDataSourceIsValid) {
    EXPECT_TRUE(Object{nil}.is_valid());
    EXPECT_TRUE(Object{0}.is_valid());
}

TEST(Object, GetId) {
    EXPECT_NE(Object(nil).id().to_str(), "");
    EXPECT_NE(Object(true).id().to_str(), "");
    EXPECT_NE(Object(-1).id().to_str(), "");
    EXPECT_NE(Object(1UL).id().to_str(), "");
    EXPECT_NE(Object(2.718).id().to_str(), "");
    EXPECT_NE(Object("tea").id().to_str(), "");
    EXPECT_NE(Object(Object::LIST).id().to_str(), "");
    EXPECT_NE(Object(Object::OMAP).id().to_str(), "");
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

TEST(Object, CompareNull) {
    EXPECT_TRUE(Object{nil} == Object{nil});
    EXPECT_EQ(Object{nil}.operator <=> (nil), std::partial_ordering::equivalent);

    Object a{nil};
    EXPECT_FALSE(a == Object{1});

    try {
        Object a{nil};
        a <=> Object{1};
        FAIL();
    } catch (...) {
    }
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
  EXPECT_FALSE(a != b);
  EXPECT_TRUE(a == b);
  a = false;
  EXPECT_TRUE(a < b);
  EXPECT_TRUE(b > a);
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

TEST(Object, CompareBoolUInt) {
    Object a{true};
    Object b{1UL};
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

    EXPECT_FALSE(a == b);
    EXPECT_FALSE(b == a);

    try {
        EXPECT_TRUE(a > b);
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
    Object b = "[1]"_json;

    EXPECT_FALSE(a == b);
    EXPECT_FALSE(b == a);

    try {
        EXPECT_FALSE(a > b);
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
  Object b{1UL};
  EXPECT_TRUE(a == b);
  EXPECT_EQ(a.operator <=> (b), std::partial_ordering::equivalent);

  a = -1;
  EXPECT_TRUE(a < b);
  EXPECT_TRUE(b > a);
  EXPECT_TRUE(a != b);
  EXPECT_TRUE(b != a);
}

TEST(Object, CompareIntUIntMax) {
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

    EXPECT_FALSE(a == b);
    EXPECT_FALSE(b == a);

    Object c{"0"};
    try {
        EXPECT_TRUE(a > c);
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
    Object b = "[1]"_json;
    EXPECT_FALSE(a == b);
    EXPECT_FALSE(b == a);

    try {
        EXPECT_FALSE(a > b);
        FAIL();
    } catch (...) {
    }

    try {
        EXPECT_FALSE(b < a);
        FAIL();
    } catch (...) {
    }
}

TEST(Object, CompareIntOrderedMap) {
    Object a{1};
    Object b = "{}"_json;

    EXPECT_TRUE(b.is_type<OrderedMap>());
    EXPECT_FALSE(a == b);
    EXPECT_FALSE(b == a);

    try {
        EXPECT_FALSE(a > b);
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

    EXPECT_FALSE(a == b);
    EXPECT_FALSE(b == a);

    Object c{"0"};
    try {
        EXPECT_TRUE(a > c);
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
    Object b = "[1]"_json;

    EXPECT_FALSE(a == b);
    EXPECT_FALSE(b == a);

    try {
        EXPECT_FALSE(a > b);
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
    Object b = "{}"_json;

    EXPECT_TRUE(b.is_type<OrderedMap>());
    EXPECT_FALSE(a == b);
    EXPECT_FALSE(b == a);

    try {
        EXPECT_FALSE(a > b);
        FAIL();
    } catch (...) {
    }

    try {
        EXPECT_FALSE(b < a);
        FAIL();
    } catch (...) {
    }
}

TEST(Object, CompareFloat) {
    Object a{3.14};
    Object b{3.14};
    EXPECT_TRUE(a == b);
    EXPECT_TRUE(b == a);

    b = 3.141;
    EXPECT_TRUE(a < b);
    EXPECT_TRUE(b > a);
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
    Object b = "[1]"_json;

    EXPECT_FALSE(a == b);
    EXPECT_FALSE(b == a);

    try {
        EXPECT_FALSE(a > b);
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
    Object b = "{}"_json;

    EXPECT_TRUE(b.is_type<OrderedMap>());
    EXPECT_FALSE(a == b);
    EXPECT_FALSE(b == a);

    try {
        EXPECT_FALSE(a > b);
        FAIL();
    } catch (...) {
    }

    try {
        EXPECT_FALSE(b < a);
        FAIL();
    } catch (...) {
    }
}

TEST(Object, CompareListList) {
    Object a = "['Assam', 'Darjeeling']"_json;
    Object b = "['Assam', 'Darjeeling']"_json;
    Object c = "['Assam', 'Ceylon']"_json;
    EXPECT_EQ(a, b);
    EXPECT_NE(a, c);
}

TEST(Object, CopyBasic) {
    EXPECT_TRUE(Object{nil}.copy() == nil);
    EXPECT_TRUE(Object{-1}.copy().is_type<Int>());
    EXPECT_TRUE(Object{1UL}.copy().is_type<UInt>());
    EXPECT_TRUE(Object{2.718}.copy().is_type<Float>());
    EXPECT_TRUE(Object{"tea"}.copy().is_type<String>());

    EXPECT_EQ(Object{nil}.copy(), nil);
    EXPECT_EQ(Object{-1}.copy(), -1);
    EXPECT_EQ(Object{1UL}.copy(), 1UL);
    EXPECT_EQ(Object{2.718}.copy(), 2.718);
    EXPECT_EQ(Object{"tea"}.copy(), "tea");

    try {
        Object obj;
        obj.copy();
        FAIL();
    } catch (...) {
    }
}

TEST(Object, ListGet) {
  Object obj = "[7, 8, 9]"_json;
  EXPECT_TRUE(obj.is_type<ObjectList>());
  EXPECT_EQ(obj.get(0).cast<Int>(), 7);
  EXPECT_EQ(obj.get(1).cast<Int>(), 8);
  EXPECT_EQ(obj.get(2).cast<Int>(), 9);
  EXPECT_EQ(obj.get(-1).cast<Int>(), 9);
  EXPECT_EQ(obj.get(-2).cast<Int>(), 8);
  EXPECT_EQ(obj.get(-3).cast<Int>(), 7);
  EXPECT_TRUE(obj.get(-4) == nil);
  EXPECT_TRUE(obj.get(-5) == nil);
  EXPECT_TRUE(obj.get(3) == nil);
  EXPECT_TRUE(obj.get(4) == nil);
}

TEST(Object, ListGetSlice) {
  Object obj = "[10, 11, 12, 13, 14, 15, 16, 17, 18, 19]"_json;
  Object list = obj.get("0:2"_slice);
  ASSERT_EQ(list.size(), 2UL);
  EXPECT_EQ(list.get(0), 10);
  EXPECT_EQ(list.get(1), 11);

  list = obj.get("0:-8"_slice);
  ASSERT_EQ(list.size(), 2UL);
  EXPECT_EQ(list.get(0), 10);
  EXPECT_EQ(list.get(1), 11);

  list = obj.get("-2"_slice);
  ASSERT_EQ(list.size(), 1UL);
  EXPECT_EQ(list.get(0), 18);

  list = obj.get("-2:"_slice);
  ASSERT_EQ(list.size(), 2UL);
  EXPECT_EQ(list.get(0), 18);
  EXPECT_EQ(list.get(1), 19);

  list = obj.get(":1"_slice);
  ASSERT_EQ(list.size(), 1UL);
  EXPECT_EQ(list.get(0), 10);

  list = obj.get(":"_slice);
  ASSERT_EQ(list.size(), 10UL);
  EXPECT_EQ(list.get(0), 10);
  EXPECT_EQ(list.get(9), 19);

  list = obj.get("-7::-1"_slice);
  ASSERT_EQ(list.size(), 4UL);
  EXPECT_EQ(list.get(0), 13);
  EXPECT_EQ(list.get(1), 12);
  EXPECT_EQ(list.get(2), 11);
  EXPECT_EQ(list.get(3), 10);

  list = obj.get("::-1"_slice);
  ASSERT_EQ(list.size(), 10UL);
  EXPECT_EQ(list.get(0), 19);
  EXPECT_EQ(list.get(9), 10);

  list = obj.get("::-4"_slice);
  ASSERT_EQ(list.size(), 3UL);
  EXPECT_EQ(list.get(0), 19);
  EXPECT_EQ(list.get(1), 15);
  EXPECT_EQ(list.get(2), 11);

  list = obj.get("-100:"_slice);
  ASSERT_EQ(list.size(), 10UL);
  EXPECT_EQ(list.get(0), 10);
  EXPECT_EQ(list.get(9), 19);
}

TEST(Object, ListGetOutOfRange) {
    Object obj = json::parse(R"([])");
    EXPECT_TRUE(obj.is_type<ObjectList>());
    EXPECT_TRUE(obj.get(1) == nil);
}

TEST(Object, ListSet) {
    Object obj = "[1, 2, 3]"_json;
    obj.set(1, 12);
    obj.set(-1, 13);
    EXPECT_EQ(obj.get(0), 1);
    EXPECT_EQ(obj.get(1), 12);
    EXPECT_EQ(obj.get(2), 13);

    Key one = 1;
    obj.set(one, 102);
    Key minus_one = -1;
    obj.set(minus_one, 103);
    EXPECT_EQ(obj.get(0), 1);
    EXPECT_EQ(obj.get(1), 102);
    EXPECT_EQ(obj.get(2), 103);
}

TEST(Object, ListSetSlice) {
  Object obj = "[10, 11, 12, 13, 14]"_json;

  obj.set("1:3"_slice, "[101, 102]"_json);
  ASSERT_EQ(obj.size(), 5UL);
  EXPECT_EQ(obj.get(0), 10);
  EXPECT_EQ(obj.get(1), 101);
  EXPECT_EQ(obj.get(2), 102);
  EXPECT_EQ(obj.get(3), 13);
  EXPECT_EQ(obj.get(4), 14);

  obj.set("1:4"_slice, "[1001, 1002]"_json);
  ASSERT_EQ(obj.size(), 4UL);
  EXPECT_EQ(obj.get(0), 10);
  EXPECT_EQ(obj.get(1), 1001);
  EXPECT_EQ(obj.get(2), 1002);
  EXPECT_EQ(obj.get(3), 14);

  obj.set("1:2"_slice, "[10001, 10002]"_json);
  ASSERT_EQ(obj.size(), 5UL);
  EXPECT_EQ(obj.get(0), 10);
  EXPECT_EQ(obj.get(1), 10001);
  EXPECT_EQ(obj.get(2), 10002);
  EXPECT_EQ(obj.get(3), 1002);
  EXPECT_EQ(obj.get(4), 14);

  obj = "[10, 11, 12, 13, 14]"_json;
  obj.set(":"_slice, "[100, 101, 102, 103, 104]"_json);
  ASSERT_EQ(obj.size(), 5UL);
  EXPECT_EQ(obj.get(0), 100);
  EXPECT_EQ(obj.get(1), 101);
  EXPECT_EQ(obj.get(2), 102);
  EXPECT_EQ(obj.get(3), 103);
  EXPECT_EQ(obj.get(4), 104);

  obj = "[[1], [2], [3], [4], [5]]"_json;
  obj.set("::2"_slice, "[[10], [30], [50]]"_json);
  ASSERT_EQ(obj.size(), 5UL);
  EXPECT_EQ(obj.get(0).get(0), 10);
  EXPECT_EQ(obj.get(1).get(0), 2);
  EXPECT_EQ(obj.get(2).get(0), 30);
  EXPECT_EQ(obj.get(3).get(0), 4);
  EXPECT_EQ(obj.get(4).get(0), 50);
  EXPECT_TRUE(obj.get(0).parent().is(obj));
  EXPECT_TRUE(obj.get(2).parent().is(obj));
  EXPECT_TRUE(obj.get(4).parent().is(obj));

  obj = "[10, 11, 12, 13, 14]"_json;
  obj.set("1::3"_slice, "[101, 104, 107]"_json);
  ASSERT_EQ(obj.size(), 5UL);
  EXPECT_EQ(obj.get(0), 10);
  EXPECT_EQ(obj.get(1), 101);
  EXPECT_EQ(obj.get(2), 12);
  EXPECT_EQ(obj.get(3), 13);
  EXPECT_EQ(obj.get(4), 104);

  obj = "[10, 11, 12, 13, 14]"_json;
  obj.set("3:1:-1"_slice, "[103, 102]"_json);
  ASSERT_EQ(obj.size(), 5UL);
  EXPECT_EQ(obj.get(0), 10);
  EXPECT_EQ(obj.get(1), 11);
  EXPECT_EQ(obj.get(2), 102);
  EXPECT_EQ(obj.get(3), 103);
  EXPECT_EQ(obj.get(4), 14);

  obj = "[10, 11, 12, 13, 14]"_json;
  obj.set("3:1:-1"_slice, "[103, 102, 101]"_json);
  ASSERT_EQ(obj.size(), 6UL);
  EXPECT_EQ(obj.get(0), 10);
  EXPECT_EQ(obj.get(1), 11);
  EXPECT_EQ(obj.get(2), 101);
  EXPECT_EQ(obj.get(3), 102);
  EXPECT_EQ(obj.get(4), 103);
  EXPECT_EQ(obj.get(5), 14);

  obj = "[10, 11, 12, 13, 14]"_json;
  obj.set("1::-1"_slice, "[103, 102, 101]"_json);
  ASSERT_EQ(obj.size(), 6UL);
  EXPECT_EQ(obj.get(0), 101);
  EXPECT_EQ(obj.get(1), 102);
  EXPECT_EQ(obj.get(2), 103);
  EXPECT_EQ(obj.get(3), 12);
  EXPECT_EQ(obj.get(4), 13);
  EXPECT_EQ(obj.get(5), 14);

  obj = "[10, 11, 12, 13, 14]"_json;
  obj.set("::-1"_slice, "[103, 102, 101]"_json);
  ASSERT_EQ(obj.size(), 3UL);
  EXPECT_EQ(obj.get(0), 101);
  EXPECT_EQ(obj.get(1), 102);
  EXPECT_EQ(obj.get(2), 103);

  obj = "[10, 11, 12, 13, 14]"_json;
  obj.set("::-2"_slice, "[104, 102, 100]"_json);
  ASSERT_EQ(obj.size(), 5UL);
  EXPECT_EQ(obj.get(0), 100);
  EXPECT_EQ(obj.get(1), 11);
  EXPECT_EQ(obj.get(2), 102);
  EXPECT_EQ(obj.get(3), 13);
  EXPECT_EQ(obj.get(4), 104);

  obj = "[10, 11, 12, 13, 14]"_json;
  obj.set("::-2"_slice, "[104, 102, 100, 90]"_json);
  ASSERT_EQ(obj.size(), 5UL);
  EXPECT_EQ(obj.get(0), 100);
  EXPECT_EQ(obj.get(1), 11);
  EXPECT_EQ(obj.get(2), 102);
  EXPECT_EQ(obj.get(3), 13);
  EXPECT_EQ(obj.get(4), 104);

  obj = "[10, 11, 12, 13, 14]"_json;
  obj.set("::-2"_slice, "[104, 102]"_json);
  ASSERT_EQ(obj.size(), 4UL);
  EXPECT_EQ(obj.get(0), 11);
  EXPECT_EQ(obj.get(1), 102);
  EXPECT_EQ(obj.get(2), 13);
  EXPECT_EQ(obj.get(3), 104);
}

TEST(Object, ListDelete) {
    Object obj = "[1, 2, 3]"_json;
    obj.del(0);
    EXPECT_EQ(obj.size(), 2UL);
    EXPECT_EQ(obj.get(0), 2);
    EXPECT_EQ(obj.get(1), 3);
    obj.del(-1);
    EXPECT_EQ(obj.size(), 1UL);
    EXPECT_EQ(obj.get(0), 2);
}

TEST(Object, OrderedMapGet) {
  Object obj = json::parse(R"({0: 7, 1: 8, 2: 9, "name": "Brian"})");
  EXPECT_TRUE(obj.is_type<OrderedMap>());
  EXPECT_EQ(obj.get(0).cast<Int>(), 7);
  EXPECT_EQ(obj.get(1).cast<Int>(), 8);
  EXPECT_EQ(obj.get(2).cast<Int>(), 9);
  EXPECT_EQ(obj.get("name"_key).as<String>(), "Brian");
  EXPECT_TRUE(obj.get("blah"_key) == nil);
}

TEST(Object, SortedMapGet) {
  json::Options options; options.use_sorted_map = true;
  Object obj = json::parse(options, R"({0: 7, 1: 8, 2: 9, "name": "Brian"})");
  EXPECT_TRUE(obj.is_type<SortedMap>());
  EXPECT_EQ(obj.get(0).cast<Int>(), 7);
  EXPECT_EQ(obj.get(1).cast<Int>(), 8);
  EXPECT_EQ(obj.get(2).cast<Int>(), 9);
  EXPECT_EQ(obj.get("name"_key).as<String>(), "Brian");
  EXPECT_TRUE(obj.get("blah"_key) == nil);
}

TEST(Object, OrderedMapGetNotFound) {
    Object obj = json::parse(R"({})");
    EXPECT_TRUE(obj.is_type<OrderedMap>());
    EXPECT_TRUE(obj.get("x"_key) == nil);

    obj.set("x"_key, "X");
    EXPECT_FALSE(obj.get("x"_key) == nil);

    obj.del("x"_key);
    EXPECT_TRUE(obj.get("x"_key) == nil);
}

TEST(Object, SortedMapSetSlice) {
    Object obj = Object::SMAP;
    obj.set("0:3"_slice, "[1, 2, 3]"_json);
    ASSERT_EQ(obj.size(), 3UL);
    EXPECT_EQ(obj.get(0), 1);
    EXPECT_EQ(obj.get(1), 2);
    EXPECT_EQ(obj.get(2), 3);
}

TEST(Object, SortedMapGetNotFound) {
    json::Options options; options.use_sorted_map = true;
    Object obj = json::parse(options, R"({})");
    EXPECT_TRUE(obj.is_type<SortedMap>());
    EXPECT_TRUE(obj.get("x"_key) == nil);

    obj.set("x"_key, "X");
    EXPECT_FALSE(obj.get("x"_key) == nil);

    obj.del("x"_key);
    EXPECT_TRUE(obj.get("x"_key) == nil);
}

TEST(Object, MultipleGetOrderedMap) {
  Object obj = json::parse(R"({"a": {"b": {"c": 7}}})");
  EXPECT_TRUE(obj.is_type<OrderedMap>());
  EXPECT_EQ(obj.get("a"_key).get("b"_key).get("c"_key), 7);
}

TEST(Object, MultipleGetSortedMap) {
  json::Options options; options.use_sorted_map = true;
  Object obj = json::parse(options, R"({"a": {"b": {"c": 7}}})");
  EXPECT_TRUE(obj.is_type<SortedMap>());
  EXPECT_EQ(obj.get("a"_key).get("b"_key).get("c"_key), 7);
}

TEST(Object, OrderedMapSetNumber) {
    Object obj = "{'x': 100}"_json;
    obj.set("x"_key, 101);
    EXPECT_EQ(obj.get("x"s), 101);
}

TEST(Object, SortedMapSetNumber) {
    json::Options options; options.use_sorted_map = true;
    Object obj = json::parse(options, "{'x': 100}");
    obj.set("x"_key, 101);
    EXPECT_EQ(obj.get("x"s), 101);
}

TEST(Object, OrderedMapSetString) {
    Object obj = "{'x': ''}"_json;
    obj.set("x"_key, "salmon");
    EXPECT_EQ(obj.get("x"s), "salmon");
}

TEST(Object, SortedMapSetString) {
    json::Options options; options.use_sorted_map = true;
    Object obj = json::parse(options, "{'x': ''}");
    obj.set("x"_key, "salmon");
    EXPECT_EQ(obj.get("x"s), "salmon");
}

TEST(Object, OrderedMapSetList) {
    Object obj = "{'x': [100]}"_json;
    Object rhs = "[101]"_json;
    obj.set("x"_key, rhs);
    EXPECT_EQ(obj.get("x"s).get(0), 101);
}

TEST(Object, SortedMapSetList) {
    json::Options options; options.use_sorted_map = true;
    Object obj = json::parse(options, "{'x': [100]}");
    Object rhs = "[101]"_json;
    obj.set("x"_key, rhs);
    EXPECT_EQ(obj.get("x"s).get(0), 101);
}

TEST(Object, OrderedMapSetOrderedMap) {
    Object obj = "{'x': [100]}"_json;
    Object rhs = "{'y': 101}"_json;
    obj.set("x"s, rhs);
    EXPECT_TRUE(obj.get("x"s).is_type<OrderedMap>());
    EXPECT_EQ(obj.get("x"s).get("y"s), 101);
}

TEST(Object, SortedMapSetOrderedMap) {
    json::Options options; options.use_sorted_map = true;
    Object obj = json::parse(options, "{'x': [100]}");
    Object rhs = json::parse("{'y': 101}");
    obj.set("x"s, rhs);
    EXPECT_TRUE(obj.is_type<SortedMap>());
    EXPECT_TRUE(obj.get("x"s).is_type<OrderedMap>());
    EXPECT_EQ(obj.get("x"s).get("y"s), 101);
}

TEST(Object, SortedMapGetSlice) {
    Object obj = Object::SMAP;
    for (int i=0; i<5; i++)
        obj.set(i, i + 10);

  Object map = obj.get("0:2"_slice);
  ASSERT_EQ(map.size(), 2UL);
  EXPECT_EQ(map.get(0), 10);
  EXPECT_EQ(map.get(1), 11);

  map = obj.get(":2"_slice);
  ASSERT_EQ(map.size(), 2UL);
  EXPECT_EQ(map.get(0), 10);
  EXPECT_EQ(map.get(1), 11);

  map = obj.get("1:3"_slice);
  ASSERT_EQ(map.size(), 2UL);
  EXPECT_EQ(map.get(0), 11);
  EXPECT_EQ(map.get(1), 12);
}

TEST(Object, SortedMapSetSliceSameLen) {
    Object obj = Object::SMAP;
    for (int i=0; i<5; i++)
        obj.set(i, i + 10);

    obj.set("1:3"_slice, "[101, 102]"_json);

    ASSERT_EQ(obj.size(), 5UL);
    EXPECT_EQ(obj.get(0), 10);
    EXPECT_EQ(obj.get(1), 101);
    EXPECT_EQ(obj.get(2), 102);
    EXPECT_EQ(obj.get(3), 13);
    EXPECT_EQ(obj.get(4), 14);
}

TEST(Object, SortedMapSetSliceShorter) {
    Object obj = Object::SMAP;
    for (int i=0; i<5; i++)
        obj.set(i, i + 10);

    obj.set("1:4"_slice, "[101, 102]"_json);

    ASSERT_EQ(obj.size(), 4UL);
    EXPECT_EQ(obj.get(0), 10);
    EXPECT_EQ(obj.get(1), 101);
    EXPECT_EQ(obj.get(2), 102);
    EXPECT_EQ(obj.get(4), 14);
}

TEST(Object, SortedMapSetSliceLonger) {
    Object obj = Object::SMAP;
    for (int i=0; i<5; i++)
        obj.set(i, i + 10);

    obj.set("1:3"_slice, "[101, 102, 103]"_json);

    ASSERT_EQ(obj.size(), 5UL);
    EXPECT_EQ(obj.get(0), 10);
    EXPECT_EQ(obj.get(1), 101);
    EXPECT_EQ(obj.get(2), 102);
    EXPECT_EQ(obj.get(3), 13);
    EXPECT_EQ(obj.get(4), 14);
}

TEST(Object, SortedMapIllegalSlice) {
    Object obj = Object::SMAP;
    for (int i=0; i<5; i++)
        obj.set(i, i + 10);

    // noop
    obj.set(":"_slice, "[101, 102]"_json);

    try {
      obj.set("-2:"_slice, "[101, 102]"_json);
      FAIL();
    } catch (const std::out_of_range& ex) {
    }

    try {
      obj.set(":-1"_slice, "[101, 102]"_json);
      FAIL();
    } catch (const std::out_of_range& ex) {
    }

    try {
      obj.set("::-1"_slice, "[101, 102]"_json);
      FAIL();
    } catch (const std::out_of_range& ex) {
    }
}

TEST(Object, SortedMapDelSlice) {
    Object obj = Object::SMAP;
    for (int i=0; i<5; i++)
        obj.set(i, i + 10);

    obj.del("1:3"_slice);

    ASSERT_EQ(obj.size(), 3UL);
    EXPECT_EQ(obj.get(0), 10);
    EXPECT_EQ(obj.get(3), 13);
    EXPECT_EQ(obj.get(4), 14);
}

TEST(Object, GetParentOfEmpty) {
    Object obj;
    try {
        obj.parent();
        FAIL();
    } catch (...) {
    }
}

TEST(Object, SetDeepSetParent) {
    Object root = json::parse("{}");
    Object subtree = json::parse("{'y': {}}");
    root.set("x"_key, subtree);
    EXPECT_TRUE(subtree.parent().is(root));
    EXPECT_TRUE(subtree["y"_key].parent().is(subtree));
}

TEST(Object, SetReplaceInParent) {
    Object obj = json::parse("{'x': 'X'}");
    EXPECT_EQ(obj.get("x"_key), "X");

    Object rhs{"Y"};
    obj.get("x"_key).set(rhs);
    EXPECT_EQ(obj.get("x"_key), "Y");
}

TEST(Object, Is) {
    Object obj = json::parse("{'x': [{'u': 1}, {'u': 2}]}");
    EXPECT_TRUE(obj.get("x[1]"_path).parent().is(obj.get("x"_key)));
}

TEST(Object, OrderedMapGetKey) {
    Object obj = json::parse("{'x': 'X', 'y': 'Y', 'z': ['Z0', 'Z1']}");
    EXPECT_EQ(obj.get("x"_key).key(), "x"_key);
    EXPECT_EQ(obj.get("y"_key).key(), "y"_key);
    EXPECT_EQ(obj.get("z"_key).key(), "z"_key);
    EXPECT_EQ(obj.get("z"_key).get(0).key(), 0);
    EXPECT_EQ(obj.get("z"_key).get(1).key(), 1);
}

TEST(Object, SortedMapGetKey) {
    json::Options options; options.use_sorted_map = true;
    Object obj = json::parse(options, "{'x': 'X', 'y': 'Y', 'z': ['Z0', 'Z1']}");
    EXPECT_EQ(obj.get("x"_key).key(), "x"_key);
    EXPECT_EQ(obj.get("y"_key).key(), "y"_key);
    EXPECT_EQ(obj.get("z"_key).key(), "z"_key);
    EXPECT_EQ(obj.get("z"_key).get(0).key(), 0);
    EXPECT_EQ(obj.get("z"_key).get(1).key(), 1);
}

TEST(Object, JsonIndentation) {
    auto obj = "{'tea': 'Assam', 'country': 'India'}"_json;
    std::stringstream ss;
    obj.to_json(ss, 1);
    EXPECT_EQ(ss.str(), "{\n \"tea\": \"Assam\", \n \"country\": \"India\"\n}");
}

TEST(Object, AlienAccess) {
    std::unique_ptr<Alien> p_alien{new TestObjectAlien{}};
    Object obj{p_alien};
    EXPECT_TRUE(obj == obj);
    auto& r_any = obj.as<TestObjectAlien>();
    EXPECT_EQ(r_any.to_str(), "TestObjectAlien");
    Object map = "{}"_json;
    map["x"_key] = obj;
    EXPECT_TRUE(obj.parent().is(map));
}

TEST(Object, AlienSubscriptAccess) {
    std::unique_ptr<Alien> p_alien{new TestObjectAlien{}};
    Object obj = "{}"_json;
    obj.set("alien"_key, p_alien);
    auto& alien = obj["alien"_key].as<Alien>();
    EXPECT_EQ(alien.to_str(), "TestObjectAlien");
}

TEST(Object, AlienToStr) {
    std::unique_ptr<Alien> p_alien{new TestObjectAlien{}};
    Object obj{"{}"_json};
    obj["alien"_key] = p_alien;
    EXPECT_EQ(obj.get("alien"_key).to_str(), "TestObjectAlien");
}

TEST(Object, AlienToJson) {
    std::unique_ptr<Alien> p_alien{new TestObjectAlien{}};
    Object obj{"{}"_json};
    obj["alien"_key] = p_alien;
    EXPECT_EQ(obj.get("alien"_key).to_json(), "'TestObjectAlien'");
}

TEST(Object, AlienCopy) {
    std::unique_ptr<Alien> p_alien{new TestObjectAlien{}};
    Object obj{"{}"_json};
    obj["alien"_key] = p_alien;
    Object alien = obj["alien"_key].copy();
    EXPECT_EQ(alien.type(), Object::ANY);
}

TEST(Object, AlienCloneOnAssign) {
    std::unique_ptr<Alien> p_alien{new TestObjectAlien{}};
    Object obj1{"{}"_json};
    auto key = "alien"_key;
    obj1[key] = p_alien;
    Object obj2{"{}"_json};
    obj2.set(key, obj1[key]);
    EXPECT_EQ(obj1.get(key).to_str(), "TestObjectAlien");
    EXPECT_EQ(obj2.get(key).to_str(), "TestObjectAlien");
}

TEST(Object, AlienSubscriptAssignClone) {
    std::unique_ptr<Alien> p_alien{new TestObjectAlien{}};
    Object obj1{"{}"_json};
    auto key = "alien"_key;
    obj1.set(key, p_alien);
    Object obj2{"{}"_json};
    obj2[key] = obj1[key];
    EXPECT_EQ(obj2.get(key).to_str(), "TestObjectAlien");
}

TEST(Object, SubscriptResolution) {
    Object obj = "{'x': 'tea'}"_json;
    auto sub = obj["x"_key];
    EXPECT_FALSE(test::is_resolved(sub));
    EXPECT_EQ(sub.as<String>(), "tea");
    EXPECT_TRUE(test::is_resolved(sub));
}

TEST(Object, Subscript) {
    Object obj = "{'x': 'tea', 'y': {'u': [1, 2], 'v': [3, 4]}}"_json;
    EXPECT_EQ(obj["x"_key], "tea");
    EXPECT_EQ(obj["y"_key]["v"_key][1], 4);
    EXPECT_EQ(obj["y"_key].key(), "y"_key);
    EXPECT_EQ(obj["y"_path]["u[1]"_path], 2);
}

TEST(Object, SubscriptAssignKey) {
    Object obj = "{'x': 'water', 'y': {'u': [1, 2], 'v': [3, 4]}}"_json;
    auto result = obj["x"_key] = "tea";
    EXPECT_EQ(obj.get("x"_key), "tea");
    EXPECT_TRUE(result.is(obj["x"_key]));

    obj["y"_key]["u"_key][1] = 7;
    EXPECT_EQ(obj.get("y.u"_path), "[1, 7]"_json);
}

TEST(Object, SubscriptAssignPath) {
    Object obj = "{'x': {'y': {'z': 'water'}}}"_json;
    obj["x.y.z"_path] = "tea";
    EXPECT_EQ(obj.get("x.y.z"_path), "tea");
}

TEST(Object, SubscriptAssignFromSubscript) {
    Object obj1 = "{'x': '1'}"_json;
    Object obj2 = "{'x': '2'}"_json;
    auto k = "x"_key;
    obj1[k] = obj2[k];
    EXPECT_EQ(obj1.get(k), obj2.get(k));
    EXPECT_EQ(obj1[k], obj2[k]);
    EXPECT_NE(obj1.get(k).id(), obj2.get(k).id());
}

TEST(Object, SubscriptDelFromParent) {
    Object obj = "{'x': 'tea'}"_json;
    auto sub = obj["x"_key];
    EXPECT_TRUE(sub.parent().is(obj));
    sub.del_from_parent();
    EXPECT_EQ(sub.parent(), nil);
}

TEST(Object, ReuseResolvedSubscript) {
    Object obj = "{'x': {'u': [0, 1], 'v': [2, 3]}, 'y': {'u': [4, 5]}}"_json;
    auto sub1 = obj["x"_key];
    auto sub2 = sub1["u"_key];
    EXPECT_EQ(sub1["v[1]"_path], 3);
    EXPECT_EQ(sub2[1], 1);
}

TEST(Object, SubscriptGetKey) {
    Object obj = "{'x': 'tea'}"_json;
    EXPECT_EQ(obj["x"_key].key(), "x");
}

TEST(Object, KeyOf) {
    Object obj = json::parse(
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
    EXPECT_TRUE(Object{nil}.key_of(obj) == nil);
    EXPECT_EQ(obj.key_of(obj.get("bool"_key)), "bool"_key);
    EXPECT_EQ(obj.key_of(obj.get("int"_key)), "int"_key);
    EXPECT_EQ(obj.key_of(obj.get("uint"_key)), "uint"_key);
    EXPECT_EQ(obj.key_of(obj.get("float"_key)), "float"_key);
    EXPECT_EQ(obj.key_of(obj.get("str"_key)), "str"_key);
    EXPECT_EQ(obj.key_of(obj.get("list"_key)), "list"_key);
    EXPECT_EQ(obj.key_of(obj.get("map"_key)), "map"_key);
    EXPECT_EQ(obj.key_of(obj.get("redun_bool"_key)), "bool"_key);
    EXPECT_EQ(obj.key_of(obj.get("redun_int"_key)), "int"_key);
    EXPECT_EQ(obj.key_of(obj.get("redun_uint"_key)), "uint"_key);
    EXPECT_EQ(obj.key_of(obj.get("redun_float"_key)), "float"_key);
    EXPECT_EQ(obj.key_of(obj.get("okay_str"_key)), "okay_str"_key);
}

TEST(Object, KeyOfWrongType) {
    Object parent = 7;
    Object child = 0;
    try {
        parent.key_of(child);
        FAIL();
    } catch (...) {
    }
}

TEST(Object, LineageRange) {
    Object obj = json::parse(R"({"a": {"b": ["Assam", "Ceylon"]}})");
    ObjectList ancestors;
    for (auto anc : obj.get("a"_key).get("b"_key).get(1).iter_ancestor())
        ancestors.push_back(anc);
    EXPECT_EQ(ancestors.size(), 4UL);
    EXPECT_TRUE(ancestors[0].is(obj.get("a"_key).get("b"_key).get(1)));
    EXPECT_TRUE(ancestors[1].is(obj.get("a"_key).get("b"_key)));
    EXPECT_TRUE(ancestors[2].is(obj.get("a"_key)));
    EXPECT_TRUE(ancestors[3].is(obj));
}

TEST(Object, ListChildrenRange) {
    Object obj = json::parse(R"([true, 1, "x"])");
    ObjectList children;
    for (auto obj : obj.iter_values())
        children.push_back(obj);
    EXPECT_EQ(children.size(), 3UL);
    EXPECT_TRUE(children[0].is_type<bool>());
    EXPECT_TRUE(children[1].is_type<Int>());
    EXPECT_TRUE(children[2].is_type<String>());
}

TEST(Object, OrderedMapChildrenRange) {
    Object obj = json::parse(R"({"a": true, "b": 1, "c": "x"})");
    ObjectList children;
    for (auto obj : obj.iter_values())
        children.push_back(obj);
    EXPECT_EQ(children.size(), 3UL);
    EXPECT_TRUE(children[0].is_type<bool>());
    EXPECT_TRUE(children[1].is_type<Int>());
    EXPECT_TRUE(children[2].is_type<String>());
}

TEST(Object, SortedMapChildrenRange) {
    json::Options options; options.use_sorted_map = true;
    Object obj = json::parse(options, R"({"b": 1, "a": true, "c": "x"})");
    ObjectList children;
    for (auto obj : obj.iter_values())
        children.push_back(obj);
    EXPECT_EQ(children.size(), 3UL);
    EXPECT_TRUE(children[0].is_type<bool>());
    EXPECT_TRUE(children[1].is_type<Int>());
    EXPECT_TRUE(children[2].is_type<String>());
}

TEST(Object, TreeRangeOverOrderedMaps) {
    Object obj = json::parse(R"({
        "a": {"aa": "AA", "ab": "AB"}, 
        "b": [{"b0a": "B0A", "b0b": "B0B"}, 
              {"b1a": "B1A", "b1b": ["B1B0"], "b1c": {"b1ca": "B1CA"}},
              [], "B2"]
    })");
    ObjectList list;
    for (auto des : obj.iter_tree())
        list.push_back(des);
    ASSERT_EQ(list.size(), 16UL);
    EXPECT_TRUE(list[0].is(obj));
    EXPECT_TRUE(list[1].is(obj.get("a"_key)));
    EXPECT_TRUE(list[7].is(obj.get("b"_key).get(2)));
    EXPECT_TRUE(list[8].is(obj.get("b"_key).get(3)));
    EXPECT_EQ(list[9], "B0A");
    EXPECT_EQ(list[15], "B1CA");
}

TEST(Object, TreeRangeOverSortedMaps) {
    json::Options options; options.use_sorted_map = true;
    Object obj = json::parse(options, R"({
        "b": [{"b0a": "B0A", "b0b": "B0B"}, 
              {"b1b": ["B1B0"], "b1a": "B1A", "b1c": {"b1ca": "B1CA"}},
              [], "B2"],
        "a": {"ab": "AB", "aa": "AA"} 
    })");
    ObjectList list;
    for (auto des : obj.iter_tree())
        list.push_back(des);
    ASSERT_EQ(list.size(), 16UL);
    EXPECT_TRUE(list[0].is(obj));
    EXPECT_TRUE(list[1].is(obj.get("a"_key)));
    EXPECT_TRUE(list[7].is(obj.get("b"_key).get(2)));
    EXPECT_TRUE(list[8].is(obj.get("b"_key).get(3)));
    EXPECT_EQ(list[9], "B0A");
    EXPECT_EQ(list[15], "B1CA");
}

TEST(Object, TreeRangeVisitPredOverOrderedMaps) {
    Object obj = json::parse(R"({
        "a": {"aa": "AA", "ab": "AB"}, 
        "b": [{"b0a": "B0A", "b0b": "B0B"}, 
              {"b1a": "B1A", "b1b": ["B1B0"], "b1c": {"b1ca": "B1CA"}},
              [], "B2"]
    })");
    ObjectList list;
    auto pred = [](const Object& o) { return o.is_type<String>() && o.as<String>()[0] == 'B'; };
    for (auto des : obj.iter_tree(pred))
        list.push_back(des);
    ASSERT_EQ(list.size(), 6UL);
    EXPECT_TRUE(list[0].is(obj.get("b"_key).get(3)));
    EXPECT_TRUE(list[1].is(obj.get("b"_key).get(0).get("b0a"_key)));
    EXPECT_TRUE(list[5].is(obj.get("b"_key).get(1).get("b1c"_key).get("b1ca"_key)));
}

TEST(Object, TreeRangeVisitPredOverSortedMaps) {
    json::Options options; options.use_sorted_map = true;
    Object obj = json::parse(options, R"({
        "b": [{"b0a": "B0A", "b0b": "B0B"}, 
              {"b1b": ["B1B0"], "b1a": "B1A", "b1c": {"b1ca": "B1CA"}},
              [], "B2"],
        "a": {"ab": "AB", "aa": "AA"} 
    })");
    ObjectList list;
    auto pred = [](const Object& o) { return o.is_type<String>() && o.as<String>()[0] == 'B'; };
    for (auto des : obj.iter_tree(pred))
        list.push_back(des);
    ASSERT_EQ(list.size(), 6UL);
    EXPECT_TRUE(list[0].is(obj.get("b"_key).get(3)));
    EXPECT_TRUE(list[1].is(obj.get("b"_key).get(0).get("b0a"_key)));
    EXPECT_TRUE(list[5].is(obj.get("b"_key).get(1).get("b1c"_key).get("b1ca"_key)));
}

TEST(Object, TreeRangeEnterPredOverOrderedMaps) {
    Object obj = json::parse(R"({
        "a": {"aa": "AA", "ab": "AB"}, 
        "b": [{"b0a": "B0A", "b0b": "B0B"}, 
              {"b1a": "B1A", "b1b": ["B1B0"], "b1c": {"b1ca": "B1CA"}},
              [], "B2"]
    })");
    ObjectList list;
    auto pred = [](const Object& o) { return o.is_type<OrderedMap>(); };
    for (auto des : obj.iter_tree_if(pred))
        list.push_back(des);
    ASSERT_EQ(list.size(), 5UL);
    EXPECT_TRUE(list[0].is(obj));
    EXPECT_TRUE(list[1].is(obj.get("a"_key)));
    EXPECT_TRUE(list[2].is(obj.get("b"_key)));
    EXPECT_TRUE(list[3].is(obj.get("a"_key).get("aa"_key)));
    EXPECT_TRUE(list[4].is(obj.get("a"_key).get("ab"_key)));
}

TEST(Object, TreeRangeEnterPredOverSortedMaps) {
    json::Options options; options.use_sorted_map = true;
    Object obj = json::parse(options, R"({
        "b": [{"b0a": "B0A", "b0b": "B0B"}, 
              {"b1b": ["B1B0"], "b1a": "B1A", "b1c": {"b1ca": "B1CA"}},
              [], "B2"],
        "a": {"ab": "AB", "aa": "AA"} 
    })");
    ObjectList list;
    auto pred = [](const Object& o) { return o.is_type<SortedMap>(); };
    for (auto des : obj.iter_tree_if(pred))
        list.push_back(des);
    ASSERT_EQ(list.size(), 5UL);
    EXPECT_TRUE(list[0].is(obj));
    EXPECT_TRUE(list[1].is(obj.get("a"_key)));
    EXPECT_TRUE(list[2].is(obj.get("b"_key)));
    EXPECT_TRUE(list[3].is(obj.get("a"_key).get("aa"_key)));
    EXPECT_TRUE(list[4].is(obj.get("a"_key).get("ab"_key)));
}

TEST(Object, TreeRange_VisitAndEnterPredOverOrderedMaps) {
    Object obj = json::parse(R"({
        "a": {"aa": "AA", "ab": "AB"}, 
        "b": [{"b0a": "B0A", "b0b": "B0B"}, 
              {"b1a": "B1A", "b1b": ["B1B0"], "b1c": {"b1ca": "B1CA"}},
              [], "B2"]
    })");
    ObjectList list;
    auto visit_pred = [](const Object& o) { return o.is_type<String>(); };
    auto enter_pred = [](const Object& o) { return o.is_type<OrderedMap>(); };
    for (auto des : obj.iter_tree_if(visit_pred, enter_pred))
        list.push_back(des);
    ASSERT_EQ(list.size(), 2UL);
    EXPECT_EQ(list[0], "AA");
    EXPECT_EQ(list[1], "AB");
}

TEST(Object, TreeRange_VisitAndEnterPredOverSortedMaps) {
    json::Options options; options.use_sorted_map = true;
    Object obj = json::parse(options, R"({
        "b": [{"b0a": "B0A", "b0b": "B0B"}, 
              {"b1b": ["B1B0"], "b1a": "B1A", "b1c": {"b1ca": "B1CA"}},
              [], "B2"],
        "a": {"ab": "AB", "aa": "AA"} 
    })");
    ObjectList list;
    auto visit_pred = [](const Object& o) { return o.is_type<String>(); };
    auto enter_pred = [](const Object& o) { return o.is_type<SortedMap>(); };
    for (auto des : obj.iter_tree_if(visit_pred, enter_pred))
        list.push_back(des);
    ASSERT_EQ(list.size(), 2UL);
    EXPECT_EQ(list[0], "AA");
    EXPECT_EQ(list[1], "AB");
}

TEST(Object, ValuesRangeMultiuser) {
    Object o1 = json::parse(R"({"a": "A", "b": "B", "c": "C"})");
    Object o2 = json::parse(R"({"x": "X", "y": "Y", "z": "Z"})");
    ObjectList result;
    auto r1 = o1.iter_values();
    auto it1 = r1.begin();
    auto end1 = r1.end();
    auto r2 = o2.iter_values();
    auto it2 = r2.begin();
    auto end2 = r2.end();
    for (; it1 != end1 && it2 != end2; ++it1, ++it2) {
        result.push_back(*it1);
        result.push_back(*it2);
    }

    std::vector<std::string> expect = {"A", "X", "B", "Y", "C", "Z"};
    EXPECT_EQ(result.size(), expect.size());
    for (size_t i=0; i<result.size(); ++i) {
        EXPECT_EQ(result[i], expect[i]);
    }
}

TEST(Object, GetPathOrderedMaps) {
    Object obj = json::parse(R"({"a": {"b": ["Assam", "Ceylon"]}})");
    EXPECT_TRUE(obj.is_type<OrderedMap>());
    EXPECT_EQ(obj.get("a"_key).path().to_str(), "a");
    EXPECT_EQ(obj.get("a"_key).get("b"_key).path().to_str(), "a.b");
    EXPECT_EQ(obj.get("a"_key).get("b"_key).get(1).path().to_str(), "a.b[1]");
    EXPECT_EQ(obj.get("a"_key).get("b"_key).get(0).path().to_str(), "a.b[0]");
    OPath path = obj.get("a"_key).get("b"_key).get(1).path();
    EXPECT_EQ(obj.get(path).id(), obj.get("a"_key).get("b"_key).get(1).id());
}

TEST(Object, GetPathSortedMaps) {
    json::Options options; options.use_sorted_map = true;
    Object obj = json::parse(options, R"({"a": {"b": ["Assam", "Ceylon"]}})");
    EXPECT_TRUE(obj.is_type<SortedMap>());
    EXPECT_EQ(obj.get("a"_key).path().to_str(), "a");
    EXPECT_EQ(obj.get("a"_key).get("b"_key).path().to_str(), "a.b");
    EXPECT_EQ(obj.get("a"_key).get("b"_key).get(1).path().to_str(), "a.b[1]");
    EXPECT_EQ(obj.get("a"_key).get("b"_key).get(0).path().to_str(), "a.b[0]");
    OPath path = obj.get("a"_key).get("b"_key).get(1).path();
    EXPECT_EQ(obj.get(path).id(), obj.get("a"_key).get("b"_key).get(1).id());
}

TEST(Object, GetPartialPath) {
    Object obj = json::parse(R"({"a": {"b": {"c": ["Assam", "Ceylon"]}}})");
    auto c = obj.get("a"_key).get("b"_key).get("c"_key);
    auto path = c.path(obj.get("a"_key));
    EXPECT_EQ(path.to_str(), "b.c");
    EXPECT_TRUE(obj.get("a"_key).get(path).is(c));
}

TEST(Object, EmptyPath) {
    Object obj = "{'x': {'y': ['tea']}}"_json;
    OPath path;
    EXPECT_TRUE(obj.get(path).is(obj));
}

TEST(Object, ParentPath) {
    Object obj = "{'x': {'y': ['tea']}}"_json;
    auto path = "x.y[0]"_path;
    auto parent_path = path.parent();
    EXPECT_TRUE(obj.get(path).parent().is(obj.get(parent_path)));
}

TEST(Object, ParentPathNil) {
    Object obj = "{'x': {'y': ['tea']}}"_json;
    auto path = "x"_path;
    auto parent_path = path.parent();
    EXPECT_EQ(obj.get(parent_path), nil);
}

TEST(Object, ConstructedPathOverOrderedMaps) {
    Object obj = json::parse(R"({"a": {"b": ["Assam", "Ceylon"]}})");
    EXPECT_TRUE(obj.is_type<OrderedMap>());
    OPath path;
    path.append("a"_key);
    path.append("b"_key);
    path.append(0);
    EXPECT_EQ(obj.get(path), "Assam");
}

TEST(Object, ConstructedPathOverSortedMaps) {
    json::Options options; options.use_sorted_map = true;
    Object obj = json::parse(options, R"({"a": {"b": ["Assam", "Ceylon"]}})");
    EXPECT_TRUE(obj.is_type<SortedMap>());
    OPath path;
    path.append("a"_key);
    path.append("b"_key);
    path.append(0);
    EXPECT_EQ(obj.get(path), "Assam");
}

TEST(Object, PathParent) {
    Object obj = json::parse(R"({"a": {"b": ["Assam", "Ceylon"]}})");
    auto path = obj.get("a"_key).get("b"_key).path().parent();
    EXPECT_EQ(path.to_str(), "a");
    EXPECT_TRUE(obj.get(path).is(obj.get("a"_key)));
}

TEST(Object, ParsePath) {
    EXPECT_EQ("a.b[1]"_path.to_str(), "a.b[1]");
    EXPECT_EQ("['a']['b'][1]"_path.to_str(), "a.b[1]");
    EXPECT_EQ(R"(["a"]["b"][1])"_path.to_str(), "a.b[1]");
    EXPECT_EQ("a.b[2].c"_path.to_str(), "a.b[2].c");
    EXPECT_EQ("a.b[-1].c"_path.to_str(), "a.b[-1].c");
}

TEST(Object, ManuallyCreatePath) {
    Object obj = json::parse("{}");

    OPath path;
    path.append("a"_key);
    path.append(0);
    path.append("b"_key);
    path.create(obj, 100);

    EXPECT_EQ(path.to_str(), "a[0].b");
    EXPECT_TRUE(obj.get("a"_key).is_type<ObjectList>());
    EXPECT_EQ(obj.get("a"_key).get(0).type(), Object::OMAP);
    EXPECT_EQ(obj.get("a"_key).get(0).get("b"_key), 100);
}

TEST(Object, CreatePartialPath) {
    Object obj = json::parse("{'a': {}}");

    OPath path;
    path.append("a"_key);
    path.append("b"_key);
    path.append(0);
    path.create(obj, 100);

    EXPECT_EQ(path.to_str(), "a.b[0]");
    EXPECT_TRUE(obj.get("a"_key).is_type<OrderedMap>());
    EXPECT_TRUE(obj.get("a"_key).get("b"_key).is_type<ObjectList>());
    EXPECT_EQ(obj.get("a"_key).get("b"_key).get(0), 100);
}

TEST(Object, PathIsLeaf) {
    Object obj = json::parse("{'a': {'a': {'a': 'tea'}}, 'b': {'a': {'a': 'totally unlike tea'}}}");
    auto path = "a.a"_path;
    EXPECT_TRUE(path.is_leaf(obj.get(path)));
    EXPECT_TRUE(path.is_leaf(obj.get("b"_key).get(path)));
    EXPECT_FALSE(path.is_leaf(obj.get("b"_key)));
    EXPECT_FALSE(path.is_leaf(obj.get("b"_key).get("a"_key)));
}

TEST(Object, CreatePathCopy) {
    Object to_obj = json::parse("{}");
    Object from_obj = json::parse("{'tea': ['Assam', 'Ceylon']}");

    OPath path;
    path.append("tea"_key);
    Object copy = path.create(to_obj, from_obj.get("tea"_key));

    EXPECT_TRUE(from_obj.get("tea"_key).parent().is(from_obj));
    EXPECT_TRUE(copy.parent().is(to_obj));
    EXPECT_EQ(copy.key(), "tea"_key);
    EXPECT_EQ(to_obj.get("tea"_key).get(0), "Assam");
}

TEST(Object, DelPath) {
    Object obj = json::parse(R"({"a": {"b": ["Assam", "Ceylon"]}})");

    OPath path;
    path.append("a"_key);
    path.append("b"_key);
    path.append(0);

    obj.del(path);
    EXPECT_EQ(obj.get("a"_key).get("b"_key).size(), 1UL);
    EXPECT_EQ(obj.get("a"_key).get("b"_key).get(0), "Ceylon");
}

TEST(Object, HashPath) {
    std::hash<OPath> hash;

    OPath path1;
    path1.append("a"_key);
    path1.append("b"_key);

    OPath path2;
    path2.append("a"_key);
    path2.append("b"_key);

    OPath path3;
    path3.append("a"_key);
    path3.append("c"_key);

    EXPECT_EQ(hash(path1), hash(path1));
    EXPECT_EQ(hash(path2), hash(path2));
    EXPECT_EQ(hash(path3), hash(path3));

    EXPECT_EQ(hash(path1), hash(path2));
    EXPECT_NE(hash(path1), hash(path3));
    EXPECT_NE(hash(path2), hash(path3));
}

TEST(Object, DelFromParent) {
    Object obj = json::parse("{'x': 'X', 'y': 'Y', 'z': 'Z'}");
    obj.get("y"_key).del_from_parent();
    EXPECT_EQ(obj.size(), 2UL);
    EXPECT_EQ(obj.get("x"_key), "X");
    EXPECT_EQ(obj.get("z"_key), "Z");
}

TEST(Object, ParentUpdateRefCount) {
    Object o1 = json::parse("{'x': 'X'}");
    Object x = o1.get("x"_key);
    EXPECT_EQ(x.ref_count(), 2UL);
    Object o2 = json::parse("{}");
    o2.set('x', x);
    EXPECT_EQ(x.ref_count(), 2UL);
}

TEST(Object, CopyCtorRefCountIntegrity) {
    Object obj = json::parse("{}");
    EXPECT_EQ(obj.ref_count(), 1UL);
    Object copy{obj};
    EXPECT_EQ(obj.ref_count(), 2UL);
    EXPECT_EQ(copy.ref_count(), 2UL);
}

TEST(Object, MoveCtorRefCountIntegrity) {
    Object obj = json::parse("{}");
    Object move{std::move(obj)};
    EXPECT_EQ(move.ref_count(), 1UL);
    EXPECT_TRUE(obj.is_empty());
    EXPECT_EQ(obj.ref_count(), Object::no_ref_count);
}

TEST(Object, WalkDF) {
  Object obj = json::parse("[1, [2, [{'x': 3}, [4, 5], {'x': 6}], 7], 8]");
  std::vector<Int> expect_order{1, 2, 3, 4, 5, 6, 7, 8};
  std::vector<Int> actual_order;

  auto visitor = [&actual_order] (const Object& parent, const Key& key, const Object& object, int) -> void {
    if (!nodel::is_container(object))
      actual_order.push_back(object.cast<Int>());
  };

  WalkDF walk{obj, visitor};
  while (walk.next()) {}

  EXPECT_EQ(actual_order.size(), expect_order.size());
  EXPECT_EQ(actual_order, expect_order);
}

TEST(Object, WalkBF) {
  Object obj = json::parse("[1, [2, [3, [{'x': 4}, {'x': 5}], 6], 7], 8]");
  std::vector<Int> expect_order{1, 8, 2, 7, 3, 6, 4, 5};
  std::vector<Int> actual_order;

  auto visitor = [&actual_order] (const Object& parent, const Key& key, const Object& object) -> void {
    if (!nodel::is_container(object))
      actual_order.push_back(object.cast<Int>());
  };

  WalkBF walk{obj, visitor};
  while (walk.next()) {}

  EXPECT_EQ(actual_order.size(), expect_order.size());
  EXPECT_EQ(actual_order, expect_order);
}

TEST(Object, RefCountPrimitive) {
  Object obj{7};
  EXPECT_TRUE(obj.is_type<Int>());
  EXPECT_EQ(obj.ref_count(), Object::no_ref_count);
}

TEST(Object, RefCountNewString) {
  Object obj{"etc"};
  EXPECT_TRUE(obj.is_type<String>());
  EXPECT_EQ(obj.ref_count(), 1UL);
}

TEST(Object, RefCountNewList) {
  Object obj{ObjectList{}};
  EXPECT_TRUE(obj.is_type<ObjectList>());
  EXPECT_EQ(obj.ref_count(), 1UL);
}

TEST(Object, RefCountNewMap) {
  Object obj{OrderedMap{}};
  EXPECT_TRUE(obj.is_type<OrderedMap>());
  EXPECT_EQ(obj.ref_count(), 1UL);
}

TEST(Object, RefCountCopy) {
  Object obj{"etc"};
  Object copy1{obj};
  Object copy2{obj};
  Object* copy3 = new Object(copy2);
  EXPECT_TRUE(obj.is_type<String>());
  EXPECT_TRUE(copy1.is_type<String>());
  EXPECT_TRUE(copy2.is_type<String>());
  EXPECT_TRUE(copy3->is_type<String>());
  EXPECT_EQ(obj.ref_count(), 4UL);
  EXPECT_EQ(copy1.ref_count(), 4UL);
  EXPECT_EQ(copy2.ref_count(), 4UL);
  EXPECT_EQ(copy3->ref_count(), 4UL);
  delete copy3;
  EXPECT_EQ(obj.ref_count(), 3UL);
  EXPECT_EQ(copy1.ref_count(), 3UL);
  EXPECT_EQ(copy2.ref_count(), 3UL);
}

TEST(Object, RefCountMove) {
  Object obj{"etc"};
  Object copy{std::move(obj)};
  EXPECT_TRUE(obj.is_empty());
  EXPECT_TRUE(copy.is_type<String>());
  EXPECT_EQ(obj.ref_count(), Object::no_ref_count);
  EXPECT_EQ(copy.ref_count(), 1UL);
}

TEST(Object, RefCountCopyAssign) {
  Object obj{"etc"};
  Object copy1; copy1 = obj;
  Object copy2; copy2 = obj;
  Object* copy3 = new Object(); *copy3 = copy2;
  EXPECT_TRUE(obj.is_type<String>());
  EXPECT_TRUE(copy1.is_type<String>());
  EXPECT_TRUE(copy2.is_type<String>());
  EXPECT_TRUE(copy3->is_type<String>());
  EXPECT_EQ(obj.ref_count(), 4UL);
  EXPECT_EQ(copy1.ref_count(), 4UL);
  EXPECT_EQ(copy2.ref_count(), 4UL);
  EXPECT_EQ(copy3->ref_count(), 4UL);
  delete copy3;
  EXPECT_EQ(obj.ref_count(), 3UL);
  EXPECT_EQ(copy1.ref_count(), 3UL);
  EXPECT_EQ(copy2.ref_count(), 3UL);
}

TEST(Object, RefCountMoveAssign) {
  Object obj{"etc"};
  Object copy;
  copy = std::move(obj);
  EXPECT_TRUE(obj.is_empty());
  EXPECT_TRUE(copy.is_type<String>());
  EXPECT_EQ(obj.ref_count(), Object::no_ref_count);
  EXPECT_EQ(copy.ref_count(), 1UL);
}

TEST(Object, RefCountTemporary) {
  Object obj{Object{"etc"}};
  EXPECT_TRUE(obj.is_type<String>());
  EXPECT_EQ(obj.ref_count(), 1UL);
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
  ptr_alignment_requirement_test<IRCOMap>();
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
  Object obj{nil};
  Object num{0};
  EXPECT_NE(obj.id(), num.id());
}

TEST(Object, AssignNull) {
    Object obj{"foo"};
    EXPECT_EQ(obj.ref_count(), 1UL);
    obj = nil;
    EXPECT_TRUE(obj == nil);
}

TEST(Object, AssignBool) {
    Object obj{"foo"};
    obj = true;
    EXPECT_TRUE(obj.is_type<bool>());
}

TEST(Object, AssignInt32) {
    Object obj{"foo"};
    obj = (int32_t)1;
    EXPECT_TRUE(obj.is_type<Int>());
}

TEST(Object, AssignInt64) {
    Object obj{"foo"};
    obj = (int64_t)1;
    EXPECT_TRUE(obj.is_type<Int>());
}

TEST(Object, AssignUInt32) {
    Object obj{"foo"};
    obj = (uint32_t)1;
    EXPECT_TRUE(obj.is_type<UInt>());
}

TEST(Object, AssignUInt64) {
    Object obj{"foo"};
    obj = (uint64_t)1;
    EXPECT_TRUE(obj.is_type<UInt>());
}

TEST(Object, AssignString) {
    Object obj{7};
    obj = "foo"sv;
    EXPECT_TRUE(obj.is_type<String>());
}

TEST(Object, RedundantAssign) {
    Object obj = json::parse(R"({"x": [1], "y": [2]})");
    EXPECT_TRUE(obj.get("x"_key).is_type<ObjectList>());
    EXPECT_EQ(obj.get("x"_key).get(0), 1);
    // ref count error might not manifest with one try
    for (int i=0; i<100; i++) {
        obj.get("x"_key) = obj.get("x"_key);
        EXPECT_TRUE(obj.get("x"_key).is_type<ObjectList>());
        ASSERT_EQ(obj.get("x"_key).get(0), 1);
    }
}

TEST(Object, RootParentIsNull) {
    Object root = json::parse(R"({"x": [1], "y": [2]})");
    EXPECT_TRUE(root.parent() == nil);
}

TEST(Object, ClearParentOnListUpdate) {
    Object root{Object::LIST};
    root.set(0, std::to_string(0));

    Object first = root.get(0);
    EXPECT_TRUE(first.parent().is(root));

    root.set(0, nil);
    EXPECT_TRUE(first.parent() == nil);
}

TEST(Object, ClearParentOnMapUpdate) {
    Object root{Object::OMAP};
    root.set("0"_key, std::to_string(0));

    Object first = root.get("0"_key);
    EXPECT_TRUE(first.parent().is(root));

    root.set("0"_key, nil);
    EXPECT_TRUE(first.parent() == nil);
}

TEST(Object, CopyChildToAnotherContainer) {
    Object m1 = json::parse(R"({"x": "m1x"})");
    Object m2 = json::parse(R"({})");
    m2.set("x"_key, m1);
    EXPECT_TRUE(m1.get("x"_key).parent().is(m1));
    EXPECT_TRUE(m2.get("x"_key).parent().is(m2));
    m2.set("x"_key, "m2x");
    EXPECT_EQ(m2.get("x"_key), "m2x");
}

TEST(Object, DeepCopyChildToAnotherContainer) {
    Object m1 = json::parse(R"({"x": ["m1x0", "m1x1"]})");
    Object m2 = json::parse(R"({})");
    m2.set("x"_key, m1.get("x"_key));
    EXPECT_TRUE(m1.get("x"_key).get(1).root().is(m1));
    EXPECT_TRUE(m2.get("x"_key).get(1).root().is(m2));
    m2.get("x"_key).set(1, "m2x1");
    EXPECT_EQ(m1.get("x"_key).get(1), "m1x1");
    EXPECT_EQ(m2.get("x"_key).get(1), "m2x1");
}

TEST(Object, ParentIntegrityOnDel) {
    Object par = json::parse(R"({"x": [1], "y": [2]})");
    Object x1 = par.get("x"_key);
    Object x2 = par.get("x"_key);
    EXPECT_TRUE(x1.parent().is_type<OrderedMap>());
    EXPECT_EQ(x2.parent().id(), par.id());
    EXPECT_TRUE(x1.parent().is_type<OrderedMap>());
    EXPECT_EQ(x2.parent().id(), par.id());
    par.del("x"s);
    EXPECT_TRUE(x1.parent() == nil);
    EXPECT_TRUE(x2.parent() == nil);
}

TEST(Object, IterKeys) {
    Object obj = json::parse(R"({"x": [1], "y": [2]})");
    KeyList expect = {"x"_key, "y"_key};  // NOTE: std::vector has a gotcha: {"x", "y"} is interpreted in an unexpected way.
    KeyList actual;
    for (auto& key : obj.iter_keys())
        actual.push_back(key);
    EXPECT_EQ(actual, expect);
}

TEST(Object, IterOrderedMapKeys) {
    Object obj = json::parse(R"({"x": [1], "y": [2]})");
    KeyList expect = {"x"_key, "y"_key};  // NOTE: std::vector has a gotcha: {"x", "y"} is interpreted in an unexpected way.
    KeyList actual;
    for (auto& key : obj.iter_keys())
        actual.push_back(key);
    EXPECT_EQ(actual, expect);
}

TEST(Object, IterSortedMapKeys) {
    json::Options options; options.use_sorted_map = true;
    Object obj = json::parse(options, R"({"y": [2], "x": [1]})");
    KeyList expect = {"x"_key, "y"_key};  // NOTE: std::vector has a gotcha: {"x", "y"} is interpreted in an unexpected way.
    KeyList actual;
    for (auto& key : obj.iter_keys())
        actual.push_back(key);
    EXPECT_EQ(actual, expect);
}

TEST(Object, IterSortedMapKeysLowerBound) {
    json::Options options; options.use_sorted_map = true;
    Object obj = json::parse(options, R"({"z": [3], "y": [2], "x": [1]})");
    KeyList expect = {"y"_key, "z"_key};  // NOTE: std::vector has a gotcha: {"x", "y"} is interpreted in an unexpected way.
    KeyList actual;
    for (auto& key : obj.iter_keys({"y"_key, {}}))
        actual.push_back(key);
    EXPECT_EQ(actual, expect);
}

TEST(Object, IterSortedMapKeysUpperBound) {
    json::Options options; options.use_sorted_map = true;
    Object obj = json::parse(options, R"({"z": [3], "y": [2], "x": [1]})");
    KeyList expect = {"x"_key, "y"_key};  // NOTE: std::vector has a gotcha: {"x", "y"} is interpreted in an unexpected way.
    KeyList actual;
    for (auto& key : obj.iter_keys({{}, "z"_key}))
        actual.push_back(key);
    EXPECT_EQ(actual, expect);
}

TEST(Object, IterSortedMapValuesLowerBound) {
    json::Options options; options.use_sorted_map = true;
    Object obj = json::parse(options, R"({"z": [3], "y": [2], "x": [1]})");
    ObjectList expect = {obj.get("y"_key), obj.get("z"_key)};
    ObjectList actual;
    for (auto& value : obj.iter_values({"y"_key, {}}))
        actual.push_back(value);
    EXPECT_EQ(actual, expect);
}

TEST(Object, IterSortedMapValuesUpperBound) {
    json::Options options; options.use_sorted_map = true;
    Object obj = json::parse(options, R"({"z": [3], "y": [2], "x": [1]})");
    ObjectList expect = {obj.get("x"_key), obj.get("y"_key)};
    ObjectList actual;
    for (auto& value : obj.iter_values({{}, "z"_key}))
        actual.push_back(value);
    EXPECT_EQ(actual, expect);
}

TEST(Object, IterSortedMapItemsLowerBound) {
    json::Options options; options.use_sorted_map = true;
    Object obj = json::parse(options, R"({"z": [3], "y": [2], "x": [1]})");
    Key y = "y"_key;
    Key z = "z"_key;
    ItemList expect = {{y, obj.get(y)}, {z, obj.get(z)}};
    ItemList actual;
    for (auto& item : obj.iter_items({"y"_key, {}}))
        actual.push_back(item);
    EXPECT_EQ(actual, expect);
}

TEST(Object, IterSortedMapItemsUpperBound) {
    json::Options options; options.use_sorted_map = true;
    Object obj = json::parse(options, R"({"z": [3], "y": [2], "x": [1]})");
    Key x = "x"_key;
    Key y = "y"_key;
    ItemList expect = {{x, obj.get(x)}, {y, obj.get(y)}};
    ItemList actual;
    for (auto& item : obj.iter_items({{}, "z"_key}))
        actual.push_back(item);
    EXPECT_EQ(actual, expect);
}

TEST(Object, IterListKeys) {
    Object obj = json::parse(R"([100, 101, 102])");
    KeyList expect = {0, 1, 2};
    KeyList actual;
    for (auto& key : obj.iter_keys())
        actual.push_back(key);
    EXPECT_EQ(actual, expect);
}

TEST(Object, IterListValues) {
    Object obj = json::parse(R"([100, 101, 102])");
    ObjectList expect = {100, 101, 102};
    ObjectList actual;
    for (auto& val : obj.iter_values())
        actual.push_back(val);
    EXPECT_EQ(actual, expect);
}

TEST(Object, IterListItems) {
    Object obj = json::parse(R"([100, 101, 102])");
    ItemList expect = {{0, 100}, {1, 101}, {2, 102}};
    ItemList actual;
    for (auto& item : obj.iter_items())
        actual.push_back(item);
    EXPECT_EQ(actual, expect);
}

TEST(Object, IterListKeyOpenOpenIntSlice) {
    Object obj = json::parse(R"([100, 101, 102, 103])");
    KeyList expect = {1, 2};
    KeyList actual;
    for (auto& key : obj.iter_keys({{0, Endpoint::Kind::OPEN}, {3, Endpoint::Kind::OPEN}})) {
        actual.push_back(key);
    }
    EXPECT_EQ(actual, expect);
}

TEST(Object, IterListKeyOpenClosedIntSlice) {
    Object obj = json::parse(R"([100, 101, 102, 103])");
    KeyList expect = {1, 2, 3};
    KeyList actual;
    for (auto& key : obj.iter_keys({{0, Endpoint::Kind::OPEN}, {3, Endpoint::Kind::CLOSED}}))
        actual.push_back(key);
    EXPECT_EQ(actual, expect);
}

TEST(Object, IterListKeyClosedClosedIntSlice) {
    Object obj = json::parse(R"([100, 101, 102, 103])");
    KeyList expect = {0, 1, 2, 3};
    KeyList actual;
    for (auto& key : obj.iter_keys({{0, Endpoint::Kind::CLOSED}, {3, Endpoint::Kind::CLOSED}}))
        actual.push_back(key);
    EXPECT_EQ(actual, expect);
}

TEST(Object, Error) {
    Object obj = make_error("BADNESS!");
    EXPECT_FALSE(obj.is_valid());
    EXPECT_EQ(obj.to_str(), "BADNESS!");
    EXPECT_EQ(obj.type(), Object::ERROR);
}


struct TestSimpleSource : public DataSource
{
    TestSimpleSource(const std::string& json, Options options)
      : DataSource(Kind::COMPLETE, Origin::SOURCE), data{json::parse(json)} {
          set_options(options);
      }

    TestSimpleSource(const std::string& json)
      : TestSimpleSource(json, {}) {}

    DataSource* new_instance(const Object& target, Origin origin) const override { return new TestSimpleSource(data.to_json(), {}); }

    void configure(const Object& uri) override {
        DataSource::configure(uri);
        config = uri;
    }

    void read_type(const Object& target) override {
        read_meta_called = true;
        std::istringstream in{data.to_json()};
        json::impl::Parser parser{parse::StreamAdapter{in}};
        read_set(target, (Object::ReprIX)parser.parse_type());
    }

    void read(const Object& target) override {
        read_called = true;
        if (data.is_type<Int>() && data == 0xbad) {
            report_read_error("Invalid");
        } else {
            read_set(target, data);
        }
    }

    void write(const Object&, const Object& cache, const Object&) override {
        write_called = true;
        data = cache;
    }

    Object data;
    Object config;
    bool read_meta_called = false;
    bool read_called = false;
    bool write_called = false;
};


struct TestSparseSource : public DataSource
{
    TestSparseSource(const std::string& json, Options options)
      : DataSource(Kind::SPARSE, Object::OMAP, Origin::SOURCE), data{json::parse(json)} {
          set_options(options);
          set_mode(Mode::READ | Mode::WRITE | Mode::CLOBBER);
    }

    TestSparseSource(const std::string& json) : TestSparseSource(json, {}) {}

    DataSource* new_instance(const Object& target, Origin origin) const override { return new TestSparseSource(data.to_json(), {}); }

    void read_type(const Object& target) override {
        read_meta_called = true;
        std::istringstream in{data.to_json()};
        json::impl::Parser parser{parse::StreamAdapter{in}};
        read_set(target, (Object::ReprIX)parser.parse_type());
    }

    void read(const Object& target) override                               { read_called = true; read_set(target, data); }
    void write(const Object&, const Object& cache, const Object&) override { write_called = true; data = cache; }

    Object read_key(const Object&, const Key& k) override   { read_key_called = true; return data.get(k); }

    void write_key(const Object&, const Key& k, const Object& v) override {
        write_key_called = true;
        data.set(k, v);
    }

    void delete_key(const Object&, const Key& k) override {
        delete_key_called = true;
        data.del(k);
    }

    class TestKeyIterator : public KeyIterator
    {
      public:
        TestKeyIterator(TestSparseSource& ds, const Object& data, const Slice& slice) : m_ds{ds}, m_slice{slice} {
            auto range = data.iter_keys();
            m_it = range.begin();
            m_end = range.end();
            if (m_it == m_end) m_key = nil; else m_key = *m_it;
        }

        TestKeyIterator(TestSparseSource& ds, const Object& data) : TestKeyIterator{ds, data, {}} {}

        ~TestKeyIterator() { m_ds.iter_deleted = true; }

        bool next_impl() override { if (++m_it == m_end) { return false; } else { m_key = *m_it; return true; } }

      private:
        TestSparseSource& m_ds;
        nodel::KeyIterator m_it;
        nodel::KeyIterator m_end;
        Slice m_slice;
    };

    class TestValueIterator : public ValueIterator
    {
      public:
        TestValueIterator(TestSparseSource& ds, const Object& data, const Slice& slice) : m_ds{ds}, m_slice{slice} {
            auto range = data.iter_values();
            m_it = range.begin();
            m_end = range.end();
            if (m_it == m_end) m_value = nil; else m_value = *m_it;
        }

        TestValueIterator(TestSparseSource& ds, const Object& data) : TestValueIterator{ds, data, {}} {}

        ~TestValueIterator() { m_ds.iter_deleted = true; }

        bool next_impl() override { if (++m_it == m_end) { return false; } else { m_value = *m_it; return true; } }

      private:
        TestSparseSource& m_ds;
        nodel::ValueIterator m_it;
        nodel::ValueIterator m_end;
        Slice m_slice;
    };

    class TestItemIterator : public ItemIterator
    {
      public:
        TestItemIterator(TestSparseSource& ds, const Object& data, const Slice& slice) : m_ds{ds}, m_slice{slice} {
            auto range = data.iter_items();
            m_it = range.begin();
            m_end = range.end();
            if (m_it == m_end) m_item = {nil, nil}; else m_item = *m_it;
        }

        TestItemIterator(TestSparseSource& ds, const Object& data) : TestItemIterator{ds, data, {}} {}

        ~TestItemIterator() { m_ds.iter_deleted = true; }

        bool next_impl() override { if (++m_it == m_end) { return false; } else { m_item = *m_it; return true; } }

      private:
        TestSparseSource& m_ds;
        nodel::ItemIterator m_it;
        nodel::ItemIterator m_end;
        Slice m_slice;
    };

    std::unique_ptr<KeyIterator> key_iter() override     { return std::make_unique<TestKeyIterator>(*this, data); }
    std::unique_ptr<ValueIterator> value_iter() override { return std::make_unique<TestValueIterator>(*this, data); }
    std::unique_ptr<ItemIterator> item_iter() override   { return std::make_unique<TestItemIterator>(*this, data); }

    std::unique_ptr<KeyIterator> key_iter(const Slice& slice) override     { return std::make_unique<TestKeyIterator>(*this, data, slice); }
    std::unique_ptr<ValueIterator> value_iter(const Slice& slice) override { return std::make_unique<TestValueIterator>(*this, data, slice); }
    std::unique_ptr<ItemIterator> item_iter(const Slice& slice) override   { return std::make_unique<TestItemIterator>(*this, data, slice); }

    Object data;
    bool read_meta_called = false;
    bool read_called = false;
    bool write_called = false;
    bool read_key_called = false;
    bool write_key_called = false;
    bool delete_key_called = false;
    bool iter_deleted = false;
};


TEST(Object, TestSimpleSource_Invalid) {
    DataSource::Options options;
    options.throw_read_error = false;
    Object obj{new TestSimpleSource(std::to_string(0xbad), options)};
    EXPECT_FALSE(obj.is_valid());
}

TEST(Object, TestSimpleSource_ToBlah) {
    auto max_uint = std::numeric_limits<UInt>::max();
    auto max_uint_str = std::to_string(max_uint);
    auto make = [] (const std::string& json) { return Object{new TestSimpleSource(json)}; };
    EXPECT_EQ(make("true").cast<bool>(), true);
    EXPECT_EQ(make("-1").cast<Int>(), -1);
    EXPECT_EQ(make(max_uint_str).cast<UInt>(), max_uint);
    EXPECT_EQ(make("3.14159").cast<Float>(), 3.14159);
    EXPECT_EQ(make("'tea'").to_str(), "tea");
}

TEST(Object, TestSimpleSource_GetWithKey) {
    Object obj{new TestSimpleSource("{'x': 100}")};
    EXPECT_EQ(obj.get("x"_key), 100);
    Key key = "x"_key;
    EXPECT_EQ(obj.get(key), 100);
}

TEST(Object, TestSimpleSource_SetWithKey) {
    Object obj{new TestSimpleSource("{'x': 100}")};
    obj.set("x"_key, 11);
    EXPECT_EQ(obj.get("x"_key), 11);
    Key key = "x"_key;
    obj.set(key, 101);
    EXPECT_EQ(obj.get("x"_key), 101);
}

TEST(Object, TestSimpleSource_GetValues) {
    Object obj{new TestSimpleSource("{'x': 100, 'y': 101}")};
    auto values = algo::collect(obj.iter_values());
    EXPECT_EQ(values.size(), 2UL);
    EXPECT_EQ(values[0], 100);
    EXPECT_EQ(values[1], 101);
}

TEST(Object, TestSimpleSource_GetItems) {
    Object obj{new TestSimpleSource("{'x': 100, 'y': 101}")};
    EXPECT_EQ(obj.size(), 2UL);
    EXPECT_EQ(obj.get("x"_key), 100);
    EXPECT_EQ(obj.get("y"_key), 101);
}

TEST(Object, TestSimpleSource_RefCountCopyAssign) {
    Object obj{new TestSimpleSource(R"("foo")")};
    EXPECT_EQ(obj.ref_count(), 1UL);
    Object copy = obj;
    EXPECT_EQ(obj.ref_count(), 2UL);
    EXPECT_EQ(copy.ref_count(), 2UL);
    obj.release();
    EXPECT_EQ(copy.ref_count(), 1UL);
}

TEST(Object, TestSimpleSource_RefCountMoveAssign) {
    Object obj{new TestSimpleSource(R"("foo")")};
    EXPECT_EQ(obj.ref_count(), 1UL);
    Object copy = std::move(obj);
    EXPECT_EQ(copy.ref_count(), 1UL);
    obj.release();
    EXPECT_EQ(copy.ref_count(), 1UL);
}

TEST(Object, TestSimpleSource_GetType) {
    Object obj{new TestSimpleSource(R"({"x": 1, "y": 2})")};
    EXPECT_TRUE(obj.is_type<OrderedMap>());
    EXPECT_EQ(obj.type(), Object::OMAP);
}

TEST(Object, TestSimpleSource_Compare) {
    Object a{new TestSimpleSource("1")};
    Object b{new TestSimpleSource("2")};
    EXPECT_TRUE(a < b);
    EXPECT_TRUE(b > a);
}

TEST(Object, TestSimpleSource_WalkDF) {
  Object obj = json::parse("{}");
  obj.set("x"_key, new TestSimpleSource("[1, [2, [{'x': 3}, [4, 5], {'x': 6}], 7], 8]"));
  std::vector<Int> expect_order{1, 2, 3, 4, 5, 6, 7, 8};
  std::vector<Int> actual_order;

  auto visitor = [&actual_order] (const Object& parent, const Key& key, const Object& object, int) -> void {
    if (!nodel::is_container(object))
      actual_order.push_back(object.cast<Int>());
  };

  WalkDF walk{obj, visitor};
  while (walk.next()) {}

  EXPECT_EQ(actual_order.size(), expect_order.size());
  EXPECT_EQ(actual_order, expect_order);
}

TEST(Object, TestSimpleSource_WalkBF) {
  Object obj = json::parse("{}");
  obj.set("x"_key, json::parse("[1, [2, [3, [{'x': 4}, {'x': 5}], 6], 7], 8]"));
  std::vector<Int> expect_order{1, 8, 2, 7, 3, 6, 4, 5};
  std::vector<Int> actual_order;

  auto visitor = [&actual_order] (const Object& parent, const Key& key, const Object& object) -> void {
    if (!nodel::is_container(object))
      actual_order.push_back(object.cast<Int>());
  };

  WalkBF walk{obj, visitor};
  while (walk.next()) {}

  EXPECT_EQ(actual_order.size(), expect_order.size());
  EXPECT_EQ(actual_order, expect_order);
}

TEST(Object, TestSimpleSource_ChildParent) {
    Object obj{new TestSimpleSource(R"({"x": "X"})")};
    EXPECT_TRUE(obj.get("x"_key).parent().is(obj));
}

TEST(Object, TestSimpleSource_Read) {
    auto dsrc = new TestSimpleSource(R"("Strong, black tea")");
    Object obj{dsrc};
    EXPECT_EQ(obj, "Strong, black tea");
    EXPECT_TRUE(dsrc->read_called);
}

TEST(Object, TestSimpleSource_ToStr) {
    auto dsrc = new TestSimpleSource(R"("Strong, black tea")");
    Object obj{dsrc};
    EXPECT_EQ(obj.to_str(), "Strong, black tea");
}

TEST(Object, TestSimpleSource_IsValid) {
    auto dsrc = new TestSimpleSource(R"("Strong, black tea")");
    Object obj{dsrc};
    EXPECT_TRUE(obj.is_valid());
}

TEST(Object, TestSimpleSource_Id) {
    auto dsrc = new TestSimpleSource(R"("Tea!")");
    Object obj{dsrc};
    auto id_before = obj.id();
    EXPECT_EQ(obj.to_str(), "Tea!");
    auto id_after = obj.id();
    EXPECT_EQ(id_before, id_after);
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
    EXPECT_EQ(obj.get("tea"_key), "Ceylon tea");
    EXPECT_TRUE(dsrc->read_called);
    obj.del("tea"_key);
    obj.save();
    EXPECT_TRUE(obj.get("tea"_key) == nil);
    EXPECT_TRUE(dsrc->write_called);
}

TEST(Object, TestSimpleSource_RegisterURIScheme) {
    register_uri_scheme("simple", [] (const Object& uri, DataSource::Origin origin) {
        return new TestSimpleSource("{'x': 1, 'y': 2}");
    });

    Object obj = bind("simple://dude@place:1234/a/b/c"_uri);
    auto p_ds = obj.data_source<TestSimpleSource>();
    EXPECT_TRUE(p_ds != nullptr);
    EXPECT_EQ(p_ds->config.get("port"_key), 1234);
    EXPECT_EQ(p_ds->config.get("path"_key), "/a/b/c");
}

TEST(Object, TestSparseSource_KeyIterator) {
    auto dsrc = new TestSparseSource(R"({"x": 1, "y": 2})");
    Object obj{dsrc};

    EXPECT_FALSE(dsrc->iter_deleted);
    KeyList found;
    for (auto& key : obj.iter_keys())
        found.push_back(key);
    EXPECT_TRUE(dsrc->iter_deleted);
    ASSERT_EQ(found.size(), 2UL);
    EXPECT_EQ(found[0], "x"_key);
    EXPECT_EQ(found[1], "y"_key);
}

TEST(Object, TestSparseSource_ValueIterator) {
    auto dsrc = new TestSparseSource(R"({"x": "X", "y": "Y"})");
    Object obj{dsrc};

    EXPECT_FALSE(dsrc->iter_deleted);
    ObjectList found;
    for (auto& value : obj.iter_values())
        found.push_back(value);
    EXPECT_TRUE(dsrc->iter_deleted);
    ASSERT_EQ(found.size(), 2UL);
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
    ASSERT_EQ(found.size(), 2UL);
    EXPECT_EQ(found[0], std::make_pair("x"_key, Object{"X"}));
    EXPECT_EQ(found[1], std::make_pair("y"_key, Object{"Y"}));
}

TEST(Object, TestSparseSource_GetValues) {
    Object obj{new TestSparseSource("{'x': 100, 'y': 101}")};
    EXPECT_EQ(obj.size(), 2UL);
    EXPECT_EQ(obj.get("x"_key), 100);
    EXPECT_EQ(obj.get("y"_key), 101);
}

TEST(Object, TestSparseSource_GetItems) {
    Object obj{new TestSparseSource("{'x': 100, 'y': 101}")};
    EXPECT_EQ(obj.size(), 2UL);
    EXPECT_EQ(obj.get("x"_key), 100);
    EXPECT_EQ(obj.get("y"_key), 101);
}

TEST(Object, TestSparseSource_ReadKey) {
    auto dsrc = new TestSparseSource(R"({"x": 1, "y": 2})");
    Object obj{dsrc};
    EXPECT_EQ(obj.get("x"_key), 1);
    EXPECT_TRUE(dsrc->read_key_called);
    dsrc->read_key_called = false;
    EXPECT_EQ(obj.get("y"_key), 2);
    EXPECT_TRUE(dsrc->read_key_called);
}

TEST(Object, TestSparseSource_WriteKey) {
    auto dsrc = new TestSparseSource(R"({"x": 1, "y": 2, "z": 3})");
    test::DataSourceTestInterface test_iface{*dsrc};
    Object obj{dsrc};
    Key x = "x"_key;
    obj.set(x, 9);
    obj.set("z"_key, 10);
    EXPECT_FALSE(dsrc->write_called);
    EXPECT_FALSE(dsrc->write_key_called);
    EXPECT_EQ(test_iface.cache().get("x"_key), 9);
    EXPECT_EQ(dsrc->data.get("x"_key), 1);
    EXPECT_EQ(test_iface.cache().get("z"_key), 10);
    EXPECT_EQ(dsrc->data.get("z"_key), 3);
    obj.save();
    EXPECT_TRUE(dsrc->write_key_called);
    EXPECT_FALSE(dsrc->write_called);
    EXPECT_EQ(test_iface.cache().get("x"_key), 9);
    EXPECT_EQ(dsrc->data.get("x"_key), 9);
    EXPECT_EQ(test_iface.cache().get("z"_key), 10);
    EXPECT_EQ(dsrc->data.get("z"_key), 10);
}

TEST(Object, TestSparseSource_Write) {
    auto dsrc = new TestSparseSource(R"({"x": 1, "y": 2})");
    test::DataSourceTestInterface test_iface{*dsrc};
    Object obj{dsrc};
    obj.set(json::parse(R"({"x": 9, "y": 10})"));
    EXPECT_FALSE(dsrc->write_called);
    EXPECT_FALSE(dsrc->write_key_called);
    EXPECT_EQ(test_iface.cache().get("x"_key), 9);
    EXPECT_EQ(dsrc->data.get("x"_key), 1);
    obj.save();
    EXPECT_TRUE(dsrc->write_called);
    EXPECT_FALSE(dsrc->write_key_called);
    EXPECT_EQ(test_iface.cache().get("x"_key), 9);
    EXPECT_EQ(dsrc->data.get("x"_key), 9);
}

TEST(Object, TestSparseSource_DelKey) {
    auto dsrc = new TestSparseSource(R"({"x": 1, "y": 2, "z": 3})");
    test::DataSourceTestInterface test_iface{*dsrc};
    Object obj{dsrc};
    Key x = "x"_key;
    obj.del(x);
    obj.del("z"_key);
    EXPECT_EQ(obj.size(), 1UL);
    EXPECT_EQ(obj.get("y"_key), 2);

    obj.save();
    obj.reset();
    EXPECT_EQ(obj.size(), 1UL);
    EXPECT_EQ(obj.get("y"_key), 2);
}

TEST(Object, TestSparseSource_ResetKey) {
    auto dsrc = new TestSparseSource(R"({"x": 1, "y": 2, "z": 3})");
    Object obj{dsrc};
    obj.set("z"_key, 10);
    EXPECT_EQ(obj.get("z"_key), 10);
    obj.reset_key("z"_key);
    EXPECT_EQ(obj.get("z"_key), 3);
}

TEST(Object, TestSparseSource_GetSize) {
    auto dsrc = new TestSparseSource(R"({"x": 1, "y": 2, "z": 3})");
    Object obj{dsrc};
    EXPECT_EQ(obj.size(), 3UL);
}

TEST(Object, InterDataSourceCopy) {
    Object obj1{new TestSimpleSource("{'x': {}}")};
    Object obj2{new TestSimpleSource("{'y': {'z': 'Z'}}")};
    obj1.set("x.y"_path, obj2);
}
