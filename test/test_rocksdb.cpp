#include <gtest/gtest.h>
#include <filesystem>
#include <chrono>
#include <thread>
#include <rocksdb/db.h>
#include <fmt/core.h>

#include <nodel/rocksdb/KeyStore.h>
#include <nodel/support/Finally.h>
#include <nodel/filesystem/DefaultRegistry.h>

using namespace nodel;
namespace db = ::rocksdb;
using KeyStore = nodel::rocksdb::KeyStore;

namespace {

void build_db() {
    db::DB* db;
    db::Options options;
    options.create_if_missing = true;
    db::Status status = db::DB::Open(options, "test_data/test_db", &db);
    ASSERT(status.ok());

    status = db->Put(db::WriteOptions(), "0", "0");  // null key
    ASSERT(status.ok());

    status = db->Put(db::WriteOptions(), "1", "1");
    ASSERT(status.ok());

    status = db->Put(db::WriteOptions(), "2", "2");
    ASSERT(status.ok());

    status = db->Put(db::WriteOptions(), "3-7", "3-7");
    ASSERT(status.ok());

    status = db->Put(db::WriteOptions(), "47", "47");
    ASSERT(status.ok());

    status = db->Put(db::WriteOptions(), "53.1415926", "53.1415926");
    ASSERT(status.ok());

    status = db->Put(db::WriteOptions(), "6tea", "6tea");
    ASSERT(status.ok());

    status = db->Put(db::WriteOptions(), "6list", "7[1, 2, 3]");
    ASSERT(status.ok());

    status = db->Put(db::WriteOptions(), "6map", "7{'x': [1], 'y': [2]}");
    ASSERT(status.ok());

    delete db;
}

void delete_db() {
    std::filesystem::remove_all("test_data/test_db");

    using namespace std::chrono_literals;
    for(int retry=0; retry < 8 && std::filesystem::exists("test_data/test_db"); ++retry) {
        std::this_thread::sleep_for(250ms);
        DEBUG("retry {}", retry);
        std::filesystem::remove_all("test_data/test_db");
    }
}

TEST(KeyStore, Values) {
    build_db();
    Finally finally{ []() { delete_db(); } };

    Object kst = new KeyStore("test_data/test_db");
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

    Object kst = new KeyStore("test_data/test_db");
    kst.set("tmp_1"_key, "tmp_1");
    kst.set("tmp_2"_key, json::parse("[1, 2]"));
    kst.save();

    Object kst_2 = new KeyStore("test_data/test_db");
    EXPECT_EQ(kst_2.get("tmp_1"_key), "tmp_1");
    EXPECT_EQ(kst_2.get("tmp_2"_key).to_json(), "[1, 2]");

    kst_2.del("tmp_1"_key);
    kst_2.del("tmp_2"_key);
    kst_2.save();

    kst.reset();
    EXPECT_TRUE(kst.get("tmp_1"_key) == null);
    EXPECT_TRUE(kst.get("tmp_2"_key) == null);
}

TEST(KeyStore, IterKeys) {
    build_db();
    Finally finally{ []() { delete_db(); } };

    Object kst = new KeyStore("test_data/test_db");
    KeyList keys;
    for (auto& key : kst.iter_keys()) {
        keys.push_back(key);
    }

    EXPECT_EQ(keys.size(), 8);
    EXPECT_EQ(keys[0], false);
    EXPECT_EQ(keys[1], true);
    EXPECT_EQ(keys[2], -7);
    EXPECT_EQ(keys[3], 7UL);
    EXPECT_EQ(keys[4], 3.1415926);
    EXPECT_EQ(keys[5], "list");
    EXPECT_EQ(keys[6], "map");
    EXPECT_EQ(keys[7], "tea");
}

TEST(KeyStore, IterValues) {
    build_db();
    Finally finally{ []() { delete_db(); } };

    Object kst = new KeyStore("test_data/test_db");
    List values;
    for (auto& value : kst.iter_values()) {
        values.push_back(value);
    }

    EXPECT_EQ(values.size(), 8);
    EXPECT_EQ(values[0], false);
    EXPECT_EQ(values[1], true);
    EXPECT_EQ(values[2], -7);
    EXPECT_EQ(values[3], 7UL);
    EXPECT_EQ(values[4], 3.1415926);
    EXPECT_EQ(values[5].to_str(), "[1, 2, 3]");
    EXPECT_EQ(values[6].to_str(), R"({"x": [1], "y": [2]})");
    EXPECT_EQ(values[7], "tea");
}

TEST(KeyStore, IterItems) {
    build_db();
    Finally finally{ []() { delete_db(); } };

    Object kst = new KeyStore("test_data/test_db");
    ItemList items;
    for (auto& item : kst.iter_items()) {
        items.push_back(item);
    }

    EXPECT_EQ(items.size(), 8);
    EXPECT_EQ(items[0].first, false);
    EXPECT_EQ(items[0].second, false);
    EXPECT_EQ(items[4].first, 3.1415926);
    EXPECT_EQ(items[4].second, 3.1415926);
    EXPECT_EQ(items[5].first, "list");
    EXPECT_EQ(items[5].second.to_str(), "[1, 2, 3]");
}

TEST(KeyStore, FilesystemIntegration) {
    build_db();
    Finally finally{ []() { delete_db(); } };

    using namespace nodel::filesystem;
    auto wd = std::filesystem::current_path() / "test_data";

    Ref<Registry> r_reg = new nodel::filesystem::DefaultRegistry();
    auto regex = std::regex("test_data/test_db");
    r_reg->add_directory(regex, [] (const Object& target, const std::filesystem::path& path) {
        return new KeyStore(DataSource::Origin::SOURCE);
    });

    Object test_data = new Directory(r_reg, wd);
    ASSERT_TRUE(test_data.get("test_db"_key) != null);
    ASSERT_TRUE(test_data.get("test_db"_key).data_source<KeyStore>() != nullptr);
    EXPECT_EQ(test_data.get("test_db"_key).get("tea"_key), "tea");
}

} // end namespace
