/// @file
/// @b License: @n Apache License v2.0
/// @copyright Robert Dunnagan
#include <gtest/gtest.h>
#include <filesystem>
#include <chrono>
#include <thread>
#include <rocksdb/db.h>
#include <fmt/core.h>

#include <nodel/core.hxx>
#include <nodel/filesystem.hxx>
#include <nodel/kvdb.hxx>
#include <nodel/support/Finally.hxx>

using namespace nodel;

namespace db = ::rocksdb;
namespace nodeldb = nodel::kvdb;
using DB = nodel::kvdb::DB;

namespace {

void check_status(const db::Status& status) {
    if (!status.ok()) {
        DEBUG("!ok: {}", status.ToString());
        ASSERT(false);
    }
}

void build_db() {
    db::DB* db;
    db::Options options;
    options.create_if_missing = true;
    options.comparator = new nodel::kvdb::Comparator();
    db::Status status = db::DB::Open(options, "test_data/test.kvdb", &db);
    check_status(status);

    status = db->Put(db::WriteOptions(), nodeldb::serialize(Key{false}), nodeldb::serialize(Object{false}));
    check_status(status);

    status = db->Put(db::WriteOptions(), nodeldb::serialize(Key{true}), nodeldb::serialize(Object{true}));
    check_status(status);

    status = db->Put(db::WriteOptions(), nodeldb::serialize(Key{-7}), nodeldb::serialize(Object{-7}));
    check_status(status);

    status = db->Put(db::WriteOptions(), nodeldb::serialize(Key{7ULL}), nodeldb::serialize(Object{7ULL}));
    check_status(status);

    status = db->Put(db::WriteOptions(), nodeldb::serialize(Key{3.1415926}), nodeldb::serialize(Object{3.1415926}));
    check_status(status);

    status = db->Put(db::WriteOptions(), nodeldb::serialize("tea"_key), nodeldb::serialize(Object{"tea"}));
    check_status(status);

    status = db->Put(db::WriteOptions(), nodeldb::serialize("list"_key), nodeldb::serialize("[1, 2, 3]"_json));
    check_status(status);

    status = db->Put(db::WriteOptions(), nodeldb::serialize("map"_key), nodeldb::serialize("{'x': [1], 'y': [2]}"_json));
    check_status(status);

    delete db;
}

void delete_db() {
    std::filesystem::remove_all("test_data/test.kvdb");

    using namespace std::chrono_literals;
    for(int retry=0; retry < 8 && std::filesystem::exists("test_data/test.kvdb"); ++retry) {
        std::this_thread::sleep_for(250ms);
        DEBUG("retry {}", retry);
        std::filesystem::remove_all("test_data/test.kvdb");
    }
}

TEST(DB, Values) {
    build_db();
    Finally finally{ []() { delete_db(); } };

    Object kst = new DB("test_data/test.kvdb", DataSource::Origin::SOURCE);
    EXPECT_EQ(kst.get(true), Object{true});
    EXPECT_EQ(kst.get(-7), Object{-7});
    EXPECT_EQ(kst.get(7UL), Object{7UL});
    EXPECT_EQ(kst.get(3.1415926), Object{3.1415926});
    EXPECT_EQ(kst.get("tea"_key), Object{"tea"});
    EXPECT_EQ(kst.get("list"_key).to_json(), "[1, 2, 3]");
    EXPECT_EQ(kst.get("map"_key).to_json(), json::parse("{'x': [1], 'y': [2]}").to_json());
}

TEST(DB, Save) {
    build_db();
    Finally finally{ []() { delete_db(); } };

    Object kst = new DB("test_data/test.kvdb", DataSource::Origin::SOURCE);
    kst.set("tmp_1"_key, "tmp_1");
    kst.set("tmp_2"_key, json::parse("[1, 2]"));
    kst.save();

    Object kst_2 = new DB("test_data/test.kvdb", DataSource::Origin::SOURCE);
    EXPECT_EQ(kst_2.get("tmp_1"_key), "tmp_1");
    EXPECT_EQ(kst_2.get("tmp_2"_key).to_json(), "[1, 2]");

    kst_2.del("tmp_1"_key);
    kst_2.del("tmp_2"_key);
    kst_2.save();

    kst.reset();
    EXPECT_TRUE(kst.get("tmp_1"_key) == nil);
    EXPECT_TRUE(kst.get("tmp_2"_key) == nil);
}

TEST(DB, IterKeys) {
    build_db();
    Finally finally{ []() { delete_db(); } };

    Object kst = new DB("test_data/test.kvdb", DataSource::Origin::SOURCE);
    KeyList keys;
    for (auto& key : kst.iter_keys()) {
        keys.push_back(key);
    }

    EXPECT_EQ(keys.size(), 8UL);
    EXPECT_EQ(keys[0], -7);
    EXPECT_EQ(keys[1], false);
    EXPECT_EQ(keys[2], true);
    EXPECT_EQ(keys[3], 3.1415926);
    EXPECT_EQ(keys[4], 7UL);
    EXPECT_EQ(keys[5], "list");
    EXPECT_EQ(keys[6], "map");
    EXPECT_EQ(keys[7], "tea");
}

TEST(DB, IterValues) {
    build_db();
    Finally finally{ []() { delete_db(); } };

    Object kst = new DB("test_data/test.kvdb", DataSource::Origin::SOURCE);
    ObjectList values;
    for (auto& value : kst.iter_values()) {
        values.push_back(value);
    }

    EXPECT_EQ(values.size(), 8UL);
    EXPECT_EQ(values[0], -7);
    EXPECT_EQ(values[1], false);
    EXPECT_EQ(values[2], true);
    EXPECT_EQ(values[3], 3.1415926);
    EXPECT_EQ(values[4], 7UL);
    EXPECT_EQ(values[5].to_str(), "[1, 2, 3]");
    EXPECT_EQ(values[6].to_str(), R"({"x": [1], "y": [2]})");
    EXPECT_EQ(values[7], "tea");
}

TEST(DB, IterItems) {
    build_db();
    Finally finally{ []() { delete_db(); } };

    Object kst = new DB("test_data/test.kvdb", DataSource::Origin::SOURCE);
    ItemList items;
    for (auto& item : kst.iter_items()) {
        items.push_back(item);
    }

    EXPECT_EQ(items.size(), 8UL);
    EXPECT_EQ(items[0].first, -7);
    EXPECT_EQ(items[0].second, -7);
    EXPECT_EQ(items[3].first, 3.1415926);
    EXPECT_EQ(items[3].second, 3.1415926);
    EXPECT_EQ(items[5].first, "list");
    EXPECT_EQ(items[5].second.to_str(), "[1, 2, 3]");
}

TEST(DB, BugIterNewUnsavedDB) {
    Finally finally{ []() { delete_db(); } };

    nodel::kvdb::configure();

    Object data = "{'x': 1, 'y': 2}"_json;
    Object db = bind("kvdb://?perm=rw&path=test_data/test.kvdb", data);
    for (auto& key : db.iter_keys()) {
        DEBUG("{}", key.to_str());
    }
}

TEST(DB, FilesystemIntegration) {
    build_db();
    Finally finally{ []() { delete_db(); } };

    using namespace nodel::filesystem;
    auto wd = std::filesystem::current_path() / "test_data";

    Ref<Registry> r_reg = new nodel::filesystem::Registry{default_registry()};
    r_reg->associate<DB>(".kvdb");

    Object test_data = new Directory(r_reg, wd, DataSource::Origin::SOURCE);
    ASSERT_TRUE(test_data.get("test.kvdb"_key) != nil);
    ASSERT_TRUE(test_data.get("test.kvdb"_key).data_source<DB>() != nullptr);
    EXPECT_EQ(test_data.get("test.kvdb"_key).get("tea"_key), "tea");
}

} // end namespace
