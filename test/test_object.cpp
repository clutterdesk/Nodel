#include <gtest/gtest.h>
#include <nodel/nodel.h>

using Key = nodel::Key;
using Object = nodel::Object;
using List = nodel::List;
using Map = nodel::Map;
using WalkDF = nodel::WalkDF;
using WalkBF = nodel::WalkBF;

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
	EXPECT_TRUE(v.is_signed_int());
	EXPECT_EQ(v.to_json(), "-9223372036854775807");
}

TEST(Object, UInt64) {
	Object v{0xFFFFFFFFFFFFFFFFULL};
	EXPECT_TRUE(v.is_unsigned_int());
	EXPECT_EQ(v.to_json(), "18446744073709551615");
}

TEST(Object, Double) {
	Object v{3.141593};
	EXPECT_TRUE(v.is_double());
	EXPECT_EQ(v.to_json(), "3.141593");
}

TEST(Object, String) {
	Object v{"123"};
	EXPECT_TRUE(v.is_string());
	EXPECT_EQ(v.to_json(), "\"123\"");

	Object quoted{"a\"b"};
	EXPECT_TRUE(quoted.is_string());
	EXPECT_EQ(quoted.to_json(), "\"a\\\"b\"");
}

TEST(Object, List) {
	List base_list{Object(1), Object("tea"), Object(3.14), Object(true)};
	Object list{base_list};
	EXPECT_TRUE(list.is_list());
	EXPECT_EQ(list.to_json(), "[1, \"tea\", 3.140000, true]");
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

TEST(Object, RefCountPrimitive) {
	Object obj{7};
	EXPECT_TRUE(obj.is_signed_int());
	EXPECT_EQ(obj.ref_count(), std::numeric_limits<size_t>::max());
}

TEST(Object, RefCountNewString) {
	Object obj{"etc"};
	EXPECT_TRUE(obj.is_string());
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
	EXPECT_TRUE(obj.is_string());
	EXPECT_TRUE(copy1.is_string());
	EXPECT_TRUE(copy2.is_string());
	EXPECT_TRUE(copy3->is_string());
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
	EXPECT_TRUE(copy.is_string());
	EXPECT_EQ(obj.ref_count(), std::numeric_limits<size_t>::max());
	EXPECT_EQ(copy.ref_count(), 1);
}

TEST(Object, RefCountCopyAssign) {
	Object obj{"etc"};
	Object copy1; copy1 = obj;
	Object copy2; copy2 = obj;
	Object* copy3 = new Object(); *copy3 = copy2;
	EXPECT_TRUE(obj.is_string());
	EXPECT_TRUE(copy1.is_string());
	EXPECT_TRUE(copy2.is_string());
	EXPECT_TRUE(copy3->is_string());
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
	EXPECT_TRUE(copy.is_string());
	EXPECT_EQ(obj.ref_count(), std::numeric_limits<size_t>::max());
	EXPECT_EQ(copy.ref_count(), 1);
}

TEST(Object, RefCountTemporary) {
	Object obj{Object{"etc"}};
	EXPECT_TRUE(obj.is_string());
	EXPECT_EQ(obj.ref_count(), 1);
}

TEST(Object, ConversionBool) {
	Object obj{true};
	EXPECT_TRUE(obj.is_bool());
	EXPECT_TRUE(obj);
}

TEST(Object, ConversionSignedInt) {
	Object obj{-7};
	EXPECT_TRUE(obj.is_signed_int());
	int64_t v = obj;
	EXPECT_EQ(v, -7LL);
}

TEST(Object, ConversionUnsignedInt) {
	Object obj{7UL};
	EXPECT_TRUE(obj.is_unsigned_int());
	uint64_t v = obj;
	EXPECT_EQ(v, 7UL);
}

TEST(Object, ConversionDouble) {
	Object obj{7.3};
	EXPECT_TRUE(obj.is_double());
	double v = obj;
	EXPECT_EQ(v, 7.3);
}

TEST(Object, ConversionString) {
	Object obj{"camper"};
	EXPECT_TRUE(obj.is_string());
	EXPECT_EQ((std::string)obj, "camper");
}

TEST(Object, CompareBoolBool) {
	Object a{true};
	Object b{true};
	EXPECT_FALSE(a != b);
	EXPECT_TRUE(a == b);
	EXPECT_FALSE(a < b);
	EXPECT_FALSE(a > b);
}

TEST(Object, ToKey) {
	Object obj{"key"};
	EXPECT_TRUE(obj.is_string());
	EXPECT_EQ(std::get<std::string>(obj.to_key()), "key");
}

TEST(Object, SubscriptList) {
	Object obj = nodel::from_json("[7, 8, 9]");
	EXPECT_TRUE(obj.is_list());
	EXPECT_EQ((int64_t)obj[0], 7);
	EXPECT_EQ((int64_t)obj[1], 8);
	EXPECT_EQ((int64_t)obj[2], 9);
	EXPECT_EQ((int64_t)obj[Object{0}], 7);
	EXPECT_EQ((int64_t)obj[Object{1}], 8);
	EXPECT_EQ((int64_t)obj[Object{2}], 9);
}

TEST(Object, SubscriptMap) {
	Object obj = nodel::from_json("{0: 7, 1: 8, 2: 9, \"name\": \"Brian\"}");
	EXPECT_TRUE(obj.is_map());
	EXPECT_EQ((int64_t)obj[0], 7);
	EXPECT_EQ((int64_t)obj[1], 8);
	EXPECT_EQ((int64_t)obj[2], 9);
	EXPECT_EQ(obj["name"].as_string(), "Brian");
	EXPECT_EQ((int64_t)obj[Key{0}], 7);
	EXPECT_EQ((int64_t)obj[Key{1}], 8);
	EXPECT_EQ((int64_t)obj[Key{2}], 9);
	EXPECT_EQ((int64_t)obj[Object{0}], 7);
	EXPECT_EQ((int64_t)obj[Object{1}], 8);
	EXPECT_EQ((int64_t)obj[Object{2}], 9);
}

TEST(Object, WalkDF) {
	Object obj = nodel::from_json("[1, [2, [3, [4, 5], 6], 7], 8]");
	std::vector<int64_t> expect_order{1, 2, 3, 4, 5, 6, 7, 8};
	std::vector<int64_t> actual_order;

	auto visitor = [&actual_order] (const Object& parent, const Key& key, const Object& object, bool) -> void {
		if (!object.is_container())
			actual_order.push_back(object);
	};

	WalkDF walk{obj, visitor};
	while (walk.next()) {}

	EXPECT_EQ(actual_order.size(), expect_order.size());
	EXPECT_EQ(actual_order, expect_order);
}

TEST(Object, WalkBF) {
	Object obj = nodel::from_json("[1, [2, [3, [4, 5], 6], 7], 8]");
	std::vector<int64_t> expect_order{1, 8, 2, 7, 3, 6, 4, 5};
	std::vector<int64_t> actual_order;

	auto visitor = [&actual_order] (const Object& parent, const Key& key, const Object& object) -> void {
		if (!object.is_container())
			actual_order.push_back(object);
	};

	WalkBF walk{obj, visitor};
	while (walk.next()) {}

	EXPECT_EQ(actual_order.size(), expect_order.size());
	EXPECT_EQ(actual_order, expect_order);
}
