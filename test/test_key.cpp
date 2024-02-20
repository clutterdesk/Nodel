#include <gtest/gtest.h>
#include <fmt/core.h>
#include <nodel/impl/Key.h>
#include <tsl/ordered_map.h>

using namespace nodel;

TEST(Key, Null) {
  Key k;
  EXPECT_TRUE(k.is_null());
  EXPECT_EQ(k.to_str(), "null");
}

TEST(Key, Bool) {
  Key k{true};
  EXPECT_TRUE(k.is_bool());
  EXPECT_EQ(k.to_str(), "true");
  EXPECT_EQ(k.as_bool(), true);
}

TEST(Key, Int) {
  Int v = std::numeric_limits<Int>::min();
  Key k{v};
  EXPECT_TRUE(k.is_int());
  EXPECT_EQ(k.to_str(), int_to_str(v));
  EXPECT_EQ(k.as_int(), v);
}

TEST(Key, UInt) {
  UInt v = std::numeric_limits<UInt>::min();
  Key k{v};
  EXPECT_TRUE(k.is_uint());
  EXPECT_EQ(k.to_str(), int_to_str(v));
  EXPECT_EQ(k.as_uint(), v);
}

TEST(Key, Float) {
  double v = 8.8541878128e-12;
  Key k{v};
  EXPECT_TRUE(k.is_float());
  EXPECT_EQ(k.to_str(), "8.8541878128e-12");
  EXPECT_EQ(k.as_float(), v);

  Key k2 = -2.2250738585072020e-308;
  EXPECT_TRUE(k2.is_float());
  EXPECT_EQ(k2.to_str(), "-2.225073858507202e-308");
}

TEST(Key, StringLiteral) {
    Key k{"foo"s};
    EXPECT_TRUE(k.is_str());
    EXPECT_EQ(k.to_str(), "foo");
    EXPECT_EQ(k.as_str(), "foo");
}

TEST(Key, StringViewLiteral) {
    Key k{"foo"sv};
    EXPECT_TRUE(k.is_str());
    EXPECT_EQ(k.to_str(), "foo");
    EXPECT_EQ(k.as_str(), "foo");
    EXPECT_EQ(k, Key("foo"sv));
}

TEST(Key, String) {
    std::string s{"foo"};
    Key k{s};
    EXPECT_TRUE(k.is_str());
    EXPECT_EQ(k.to_str(), "foo");
    EXPECT_EQ(k.as_str(), "foo");
}

TEST(Key, StringView) {
    std::string s{"ooo"}; s[0] = 'f';
    std::string_view sv = s;
    Key k{sv};
    EXPECT_TRUE(k.is_str());
    EXPECT_EQ(k.to_str(), "foo");
    EXPECT_EQ(k.as_str(), "foo");
    EXPECT_EQ(k, Key("foo"sv));
}

TEST(Key, AssignNull) {
  Key k{1};
  EXPECT_TRUE(k.is_int());
  EXPECT_EQ(k.as_int(), 1);
  k = nullptr;
  EXPECT_TRUE(k.is_null());
}

TEST(Key, AssignBool) {
  Key k{7};
  EXPECT_TRUE(k.is_int());
  EXPECT_EQ(k.as_int(), 7);
  k = true;
  EXPECT_TRUE(k.is_bool());
  EXPECT_EQ(k.as_bool(), true);
}

TEST(Key, AssignInt) {
  Key k;
  EXPECT_TRUE(k.is_null());
  k = 7;
  EXPECT_TRUE(k.is_int());
  EXPECT_EQ(k.as_int(), 7);
}

TEST(Key, AssignUInt) {
    Key k;
    EXPECT_TRUE(k.is_null());
    k = 7UL;
    EXPECT_TRUE(k.is_uint());
    EXPECT_EQ(k.as_uint(), 7UL);
}

TEST(Key, AssignFloat) {
  Key k;
  EXPECT_TRUE(k.is_null());
  k = -2.2250738585072020e-308;
  EXPECT_TRUE(k.is_float());
  EXPECT_EQ(k.to_str(), "-2.225073858507202e-308");
}

TEST(Key, AssignStringLiteral) {
    Key k;
    EXPECT_TRUE(k.is_null());
    k = "foo"s;
    EXPECT_TRUE(k.is_str());
    EXPECT_EQ(k.as_str(), "foo");
}

TEST(Key, AssignStringViewLiteral) {
    Key k;
    EXPECT_TRUE(k.is_null());
    k = "foo"sv;
    EXPECT_TRUE(k.is_str());
    EXPECT_EQ(k.as_str(), "foo");
}

TEST(Key, CompareBool) {
    Key k{true};
    EXPECT_TRUE(k.is_bool());
    EXPECT_EQ(k, true);
    EXPECT_EQ(k, 1);
    EXPECT_EQ(k, 1.0);
    EXPECT_NE(k, 0.0);
    EXPECT_EQ(k, Key{true});
}

TEST(Key, CompareInt) {
    Key k{7};
    EXPECT_TRUE(k.is_int());
    EXPECT_EQ(k, 7);
    EXPECT_EQ(k, 7ULL);
    EXPECT_EQ(k, 7.0);
    EXPECT_EQ(k, Key{7});
}

TEST(Key, CompareUInt) {
    Key k{7UL};
    EXPECT_TRUE(k.is_uint());
    EXPECT_EQ(k, 7);
    EXPECT_EQ(k, 7ULL);
    EXPECT_EQ(k, 7.0);
    EXPECT_EQ(k, Key{7});
    EXPECT_EQ(k, Key{7ULL});
    EXPECT_EQ(k, Key{7.0});
}

TEST(Key, CompareFloat) {
    Key k{7.0};
    EXPECT_TRUE(k.is_float());
    EXPECT_EQ(k, 7);
    EXPECT_EQ(k, 7ULL);
    EXPECT_EQ(k, 7.0);
    EXPECT_EQ(k, Key{7});
    EXPECT_EQ(k, Key{7ULL});
    EXPECT_EQ(k, Key{7.0});
}

TEST(Key, CompareString) {
    Key k{"foo"s};
    EXPECT_TRUE(k.is_str());
    EXPECT_EQ(k, "foo"sv);
    EXPECT_EQ(k, "foo"s);
    EXPECT_EQ(k, Key{"foo"s});
    EXPECT_EQ(k, Key{"foo"sv});
}

TEST(Key, CompareStringView) {
    Key k{"foo"sv};
    EXPECT_TRUE(k.is_str());
    EXPECT_EQ(k, "foo"sv);
    EXPECT_EQ(k, "foo"s);
    EXPECT_EQ(k, Key{"foo"s});
    EXPECT_EQ(k, Key{"foo"sv});
}

TEST(Key, HashNull) {
    Key k;
    EXPECT_TRUE(k.is_null());
    EXPECT_EQ(std::hash<Key>{}(k), 0);
}

TEST(Key, HashInt) {
    Key k{7};
    EXPECT_TRUE(k.is_int());
    EXPECT_EQ(std::hash<Key>{}(k), 7);
}

TEST(Key, HashUInt) {
    Key k{7ULL};
    EXPECT_TRUE(k.is_uint());
    EXPECT_EQ(std::hash<Key>{}(k), 7);
}

TEST(Key, HashFloat) {
    Key k{-2.2250738585072020e-308};
    EXPECT_TRUE(k.is_float());
    EXPECT_NE(std::hash<Key>{}(k), 0);
}

static void dump(const char* data) {
    for (auto* p = data; *p != 0 ; p++) {
        fmt::print("{:x}", *p);
    }
    std::cout << std::endl;
}

TEST(Key, HashStringLiteral) {
    Key k{"foo"s};
    EXPECT_TRUE(k.is_str());
    EXPECT_EQ(std::hash<Key>{}(k), std::hash<std::string_view>{}("foo"));
}

TEST(Key, HashStringViewLiteral) {
    Key k{"foo"sv};
    EXPECT_TRUE(k.is_str());
    EXPECT_EQ(std::hash<Key>{}(k), std::hash<std::string_view>{}("foo"));
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

TEST(Key, ExplicitStringViewKeyMap) {
    tsl::ordered_map<Key, Key> map;
    map[Key("K7"sv)] = Key(8);
    EXPECT_EQ(map[Key("K7"sv)], 8);
}

TEST(Key, ImplicitStringViewKeyMap) {
    tsl::ordered_map<Key, Key> map;
    map["K7"sv] = 8;
    EXPECT_EQ(map["K7"sv], 8);
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
    map[true] = "TRUE"sv;
    EXPECT_EQ(map["K7"s], 8);
    EXPECT_EQ(map[7], "K7"sv);
    EXPECT_EQ(map[true], "TRUE"sv);
}