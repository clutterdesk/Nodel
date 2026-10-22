/// @file
/// @b License: @n Apache License v2.0
/// @copyright Robert Dunnagan
#include <gtest/gtest.h>
#include <fmt/core.h>
#include <nodel/core/Key.hxx>
#include <tsl/ordered_map.h>
#include <map>

using namespace nodel;

TEST(Key, Null) {
  Key k;
  EXPECT_TRUE(k == nil);
  EXPECT_EQ(k, Key{});
  EXPECT_EQ(k.to_str(), "nil");
}

TEST(Key, Bool) {
  Key k{true};
  EXPECT_TRUE(k.is_type<bool>());
  EXPECT_EQ(k.to_str(), "true");
  EXPECT_EQ(k.as<bool>(), true);
}

TEST(Key, Int) {
  Int v = std::numeric_limits<Int>::min();
  Key k{v};
  EXPECT_TRUE(k.is_type<Int>());
  EXPECT_EQ(k.to_str(), int_to_str(v));
  EXPECT_EQ(k.as<Int>(), v);
}

TEST(Key, UInt) {
  UInt v = std::numeric_limits<UInt>::min();
  Key k{v};
  EXPECT_TRUE(k.is_type<UInt>());
  EXPECT_EQ(k.to_str(), int_to_str(v));
  EXPECT_EQ(k.as<UInt>(), v);
}

TEST(Key, Float) {
  double v = 8.8541878128e-12;
  Key k{v};
  EXPECT_TRUE(k.is_type<Float>());
  EXPECT_EQ(k.to_str(), "8.8541878128e-12");
  EXPECT_EQ(k.as<Float>(), v);

  Key k2 = -2.2250738585072020e-308;
  EXPECT_TRUE(k2.is_type<Float>());
  EXPECT_EQ(k2.to_str(), "-2.2250738585072e-308");
}

TEST(Key, StringLiteral) {
    Key k{"foo"s};
    EXPECT_TRUE(k.is_type<String>());
    EXPECT_EQ(k.to_str(), "foo");
    EXPECT_EQ(k.as<StringView>(), "foo");
}

TEST(Key, String) {
    std::string s{"foo"};
    Key k{s};
    EXPECT_TRUE(k.is_type<String>());
    EXPECT_EQ(k.to_str(), "foo");
    EXPECT_EQ(k.as<StringView>(), "foo");
}

TEST(Key, AssignNull) {
  Key k{1};
  EXPECT_TRUE(k.is_type<Int>());
  EXPECT_EQ(k.as<Int>(), 1);
  k = nil;
  EXPECT_TRUE(k == nil);
}

TEST(Key, SelfAssign) {
    for (auto k : {Key{true}, Key{7}, Key{7ULL}, Key{7.5}, Key{"foo"s}}) {
        Key copy = k;
        auto& self = k;
        k = self;
        EXPECT_EQ(k.type(), copy.type());
        EXPECT_EQ(k, copy);
    }
}

TEST(Key, StringToFloat) {
    // The parse must stop at the end of the view, not at a null terminator.
    String text = "2.5" "7";
    Key k{StringView{text}.substr(0, 3)};
    EXPECT_EQ(k.to_float(), 2.5);
    EXPECT_EQ(str_to_float(StringView{text}.substr(0, 3)), 2.5);
    EXPECT_EQ(str_to_float(String(100, '0') + "1.5"), 1.5);
}

TEST(Key, AssignBool) {
  Key k{7};
  EXPECT_TRUE(k.is_type<Int>());
  EXPECT_EQ(k.as<Int>(), 7);
  k = true;
  EXPECT_TRUE(k.is_type<bool>());
  EXPECT_EQ(k.as<bool>(), true);
}

TEST(Key, AssignInt) {
  Key k;
  EXPECT_TRUE(k == nil);
  k = 7;
  EXPECT_TRUE(k.is_type<Int>());
  EXPECT_EQ(k.as<Int>(), 7);
}

TEST(Key, AssignUInt) {
    Key k;
    EXPECT_TRUE(k == nil);
    k = 7UL;
    EXPECT_TRUE(k.is_type<UInt>());
    EXPECT_EQ(k.as<UInt>(), 7UL);
}

TEST(Key, AssignFloat) {
  Key k;
  EXPECT_TRUE(k == nil);
  k = -2.2250738585072020e-308;
  EXPECT_TRUE(k.is_type<Float>());
  EXPECT_EQ(k.to_str(), "-2.2250738585072e-308");
}

TEST(Key, AssignStringLiteral) {
    Key k;
    EXPECT_TRUE(k == nil);
    k = "foo"s;
    EXPECT_TRUE(k.is_type<String>());
    EXPECT_EQ(k.as<StringView>(), "foo");
}

TEST(Key, CompareBool) {
    Key k{true};
    EXPECT_TRUE(k.is_type<bool>());
    EXPECT_EQ(k, true);
    EXPECT_EQ(k, 1);
    EXPECT_EQ(k, 1.0);
    EXPECT_NE(k, 0.0);
    EXPECT_EQ(k, Key{true});
    EXPECT_LT(k, 2);
    EXPECT_LT(k, 2ULL);
    EXPECT_LT(k, 1.1);
    EXPECT_LT(k, "tea"_key);
}

TEST(Key, CompareInt) {
    Key k{7};
    EXPECT_TRUE(k.is_type<Int>());
    EXPECT_EQ(k, 7);
    EXPECT_EQ(k, 7ULL);
    EXPECT_EQ(k, 7.0);
    EXPECT_EQ(k, Key{7});
}

TEST(Key, CompareUInt) {
    Key k{7UL};
    EXPECT_TRUE(k.is_type<UInt>());
    EXPECT_EQ(k, 7);
    EXPECT_EQ(k, 7ULL);
    EXPECT_EQ(k, 7.0);
    EXPECT_EQ(k, Key{7});
    EXPECT_EQ(k, Key{7ULL});
    EXPECT_EQ(k, Key{7.0});
}

TEST(Key, CompareFloat) {
    Key k{7.0};
    EXPECT_TRUE(k.is_type<Float>());
    EXPECT_EQ(k, 7);
    EXPECT_EQ(k, 7ULL);
    EXPECT_EQ(k, 7.0);
    EXPECT_EQ(k, Key{7});
    EXPECT_EQ(k, Key{7ULL});
    EXPECT_EQ(k, Key{7.0});
}

TEST(Key, CompareString) {
    Key k{"foo"s};
    EXPECT_TRUE(k.is_type<String>());
    EXPECT_EQ(k, "foo"s);
    EXPECT_EQ(k, Key{"foo"s});
}

TEST(Key, OrderNull) {
    Key k;
    EXPECT_TRUE((k <=> Key{}) == 0);
    for (auto& other : {Key{false}, Key{-1}, Key{0u}, Key{-1.5}, Key{"foo"s}}) {
        EXPECT_TRUE(k < other);
        EXPECT_TRUE(other > k);
    }
}

TEST(Key, SortedMapWithNullKey) {
    std::map<Key, int> map;
    map[Key{}] = 1;
    map[Key{5}] = 2;
    map[Key{"z"s}] = 3;
    EXPECT_EQ(map.size(), 3);
    EXPECT_EQ(map.begin()->first, nil);
    EXPECT_EQ(map[Key{}], 1);
}

TEST(Key, HashNull) {
    Key k;
    EXPECT_TRUE(k == nil);
    EXPECT_EQ(std::hash<Key>{}(k), 0UL);
}

TEST(Key, HashInt) {
    Key k{7};
    EXPECT_TRUE(k.is_type<Int>());
    EXPECT_EQ(std::hash<Key>{}(k), 7UL);
}

TEST(Key, HashUInt) {
    Key k{7ULL};
    EXPECT_TRUE(k.is_type<UInt>());
    EXPECT_EQ(std::hash<Key>{}(k), 7UL);
}

TEST(Key, HashFloat) {
    Key k{-2.2250738585072020e-308};
    EXPECT_TRUE(k.is_type<Float>());
    EXPECT_NE(std::hash<Key>{}(k), 0UL);
}

TEST(Key, HashStringLiteral) {
    Key k1{"foo"s};
    Key k2{"foo"s};
    EXPECT_TRUE(k1.is_type<String>());
    EXPECT_TRUE(k2.is_type<String>());
    EXPECT_EQ(std::hash<Key>{}(k1), std::hash<Key>{}(k2));
}

TEST(Key, HashEqualNumbers) {
    std::hash<Key> hash;
    EXPECT_EQ(hash(Key{1}), hash(Key{1.0}));
    EXPECT_EQ(hash(Key{1ULL}), hash(Key{1.0}));
    EXPECT_EQ(hash(Key{-7}), hash(Key{-7.0}));
    EXPECT_EQ(hash(Key{0}), hash(Key{-0.0}));
    EXPECT_EQ(hash(Key{true}), hash(Key{1.0}));
    EXPECT_EQ(hash(Key{std::numeric_limits<Int>::min()}), hash(Key{-9223372036854775808.0}));
    EXPECT_EQ(hash(Key{9223372036854775808ULL}), hash(Key{9223372036854775808.0}));
}

TEST(Key, FindFloatInIntKeyMap) {
    tsl::ordered_map<Key, int, KeyHash> map;
    map[Key{1}] = 1;
    map[Key{1.0}] = 2;
    EXPECT_EQ(map.size(), 1);
    EXPECT_EQ(map[Key{1}], 2);
    EXPECT_TRUE(map.contains(Key{1ULL}));
}

TEST(Key, CompareInternedStrings) {
    String s1 = "foo", s2 = "foo", s3 = "bar";
    Key k1{s1}, k2{s2}, k3{s3};
    EXPECT_EQ(k1, k2);
    EXPECT_EQ(k1, "foo"_key);
    EXPECT_NE(k1, k3);
    EXPECT_EQ(Key{""s}, ""_key);
}

TEST(Key, ExplicitIntKeyMap) {
    tsl::ordered_map<Key, Key> map;
    map[Key(7)] = Key(8);
    EXPECT_EQ(map[Key(7)], 8);
}

TEST(Key, ImplicitIntKeyMap) {
    tsl::ordered_map<Key, Key> map;
    map[7] = 8;
    EXPECT_EQ(map[7], 8);
}

TEST(Key, ExplicitStringLiteralKeyMap) {
    tsl::ordered_map<Key, Key> map;
    map[Key("K7"s)] = Key(8);
    EXPECT_EQ(map[Key("K7"s)], 8);
}

TEST(Key, ExplicitStringKeyMap) {
    tsl::ordered_map<Key, Key> map;
    map[Key(fmt::format("K{}", 7))] = Key(8);
    EXPECT_EQ(map[Key(fmt::format("K{}", 7))], 8);
}

TEST(Key, ImplicitStringKeyMap) {
    tsl::ordered_map<Key, Key> map;
    map["K7"s] = 8;
    EXPECT_EQ(map["K7"s], 8);
}

TEST(Key, ExplicitKeyList) {
    std::vector<Key> list = {Key(10), Key(11)};
    EXPECT_EQ(list[0], 10);
    EXPECT_EQ(list[1], 11);
}

TEST(Key, ImplicitKeyList) {
    std::vector<Key> list = {10, 11};
    EXPECT_EQ(list[0], 10);
    EXPECT_EQ(list[1], 11);
}

TEST(Key, HeterogenousKeyMap) {
    tsl::ordered_map<Key, Key> map;
    map["K7"s] = 8;
    map[7] = "K7"s;
    map[true] = "TRUE"s;
    EXPECT_EQ(map["K7"s], 8);
    EXPECT_EQ(map[7], "K7"s);
    EXPECT_EQ(map[true], "TRUE"s);
}

TEST(Key, BoolStep) {
    Key k = true;
    std::stringstream ss;
    k.to_step(ss);
    EXPECT_EQ(ss.str(), "[1]");
}

TEST(Key, IntStep) {
    Key k = 7;
    std::stringstream ss;
    k.to_step(ss);
    EXPECT_EQ(ss.str(), "[7]");
}

TEST(Key, UIntStep) {
    Key k = 0xFFFFFFFFFFFFFFFFULL;
    std::stringstream ss;
    k.to_step(ss);
    EXPECT_EQ(ss.str(), "[18446744073709551615]");
}

TEST(Key, FloatStep) {
    Key k = 7.3;
    std::stringstream ss;
    k.to_step(ss);
    EXPECT_EQ(ss.str(), "[7.3]");
}

TEST(Key, SimpleString) {
    Key k = "tea"_key;
    std::stringstream ss;
    k.to_step(ss);
    EXPECT_EQ(ss.str(), ".tea");
}

TEST(Key, StringWithDQuote) {
    Key k = "a\"b"_key;
    std::stringstream ss;
    k.to_step(ss);
    EXPECT_EQ(ss.str(), "[\"a\\\"b\"]");
}
