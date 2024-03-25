#include <gtest/gtest.h>
#include <fmt/core.h>
#include <filesystem>
#include <rocksdb/db.h>

#include <nodel/rocksdb/KeyStore.h>
#include <nodel/support/Finally.h>

using namespace nodel;
namespace db = ::rocksdb;

namespace {

void build_db() {
    db::DB* db;
    db::Options options;
    options.create_if_missing = true;
    db::Status status = db::DB::Open(options, "test_data/test_db", &db);
    assert(status.ok());

    status = db->Put(db::WriteOptions(), "0", "0");  // null key
    assert(status.ok());

    status = db->Put(db::WriteOptions(), "1", "1");
    assert(status.ok());

    status = db->Put(db::WriteOptions(), "2", "2");
    assert(status.ok());

    status = db->Put(db::WriteOptions(), "3-7", "3-7");
    assert(status.ok());

    status = db->Put(db::WriteOptions(), "47", "47");
    assert(status.ok());

    status = db->Put(db::WriteOptions(), "53.1415926", "53.1415926");
    assert(status.ok());

    status = db->Put(db::WriteOptions(), "6tea", "6tea");
    assert(status.ok());

    status = db->Put(db::WriteOptions(), "6list", "7[1, 2, 3]");
    assert(status.ok());

    status = db->Put(db::WriteOptions(), "6map", "7{'x': [1], 'y': [2]}");
    assert(status.ok());

    delete db;
}

void delete_db() {
    std::filesystem::remove_all("test_data/test_db");
}

TEST(KeyStore, Values) {
    build_db();
    Finally finally{ []() { delete_db(); } };

    Object kst = new nodel::rocksdb::KeyStore("test_data/test_db");
    EXPECT_EQ(kst.get(null), Object{null});
    EXPECT_EQ(kst.get(true), Object{true});
    EXPECT_EQ(kst.get(-7), Object{-7});
    EXPECT_EQ(kst.get(7UL), Object{7UL});
    EXPECT_EQ(kst.get(3.1415926), Object{3.1415926});
    EXPECT_EQ(kst.get("tea"_key), Object{"tea"});
    EXPECT_EQ(kst.get("list"_key).to_json(), "[1, 2, 3]");
    EXPECT_EQ(kst.get("map"_key).to_json(), json::parse("{'x': [1], 'y': [2]}").to_json());
}

TEST(KeyStore, Save) {
    build_db();
    Finally finally{ []() { delete_db(); } };

    Object kst = new nodel::rocksdb::KeyStore("test_data/test_db");
    kst.set("tmp_1"_key, "tmp_1");
    kst.set("tmp_2"_key, json::parse("[1, 2]"));
    kst.save();

    Object kst_2 = new nodel::rocksdb::KeyStore("test_data/test_db");
    EXPECT_EQ(kst_2.get("tmp_1"_key), "tmp_1");
    EXPECT_EQ(kst_2.get("tmp_2"_key).to_json(), "[1, 2]");

    kst_2.del("tmp_1"_key);
    kst_2.del("tmp_2"_key);
    kst_2.save();

    kst.reset();
    EXPECT_TRUE(kst.get("tmp_1"_key).is_null());
    EXPECT_TRUE(kst.get("tmp_2"_key).is_null());
}

} // end namespace
