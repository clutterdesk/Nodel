/// @file
/// @b License: @n Apache License v2.0
/// @copyright Robert Dunnagan
#include <gtest/gtest.h>

#include <nodel/fmt_support.hxx>
#include <nodel/core.hxx>
#include <nodel/filesystem.hxx>
#include <nodel/support/Finally.hxx>
#include <nodel/support/logging.hxx>

#include <filesystem>

using namespace nodel;
using namespace nodel::filesystem;


TEST(Zip, Read) {
    auto wd = std::filesystem::current_path() / "test_data";
    Object test_data = new Directory(new Registry{default_registry()}, wd, DataSource::Origin::SOURCE);
    auto zip = test_data.get("example.zip"_key);
    ASSERT_TRUE(zip != nil);
    ASSERT_EQ(zip.size(), 4UL);

    EXPECT_EQ(zip.get("['example.csv'][0][0]"_path), "G46VLEQFV6ZYO9GV");
    EXPECT_EQ(zip.get("['example.json']['teas'][0]"_path), "Assam");
    EXPECT_EQ(zip.get("more['example.csv'][0][0]"_path), "ONVKX941PP5IYUYN");
}

TEST(Zip, WriteFile) {
    auto wd = std::filesystem::current_path() / "test_data";
    Object test_data = new Directory(new Registry{default_registry()}, wd, DataSource::Origin::SOURCE);
    Finally finally{ [&test_data] () { test_data.del("tmp.zip"_key); test_data.save(); } };

    Object content{Object::OMAP};
    content.set("some.json"_key, "tea!");
    test_data.set("tmp.zip"_key, content);
    test_data.save();

    test_data.reset();
    EXPECT_EQ(test_data["['tmp.zip']['some.json']"_path], "tea!");
}

TEST(Zip, WriteFileInDirectory) {
    auto wd = std::filesystem::current_path() / "test_data";
    Object test_data = new Directory(new Registry{default_registry()}, wd, DataSource::Origin::SOURCE);
    Finally finally{ [&test_data] () { test_data.del("tmp.zip"_key); test_data.save(); } };

    Object content{Object::OMAP};
    content.set("tmp['some.json']"_path, "{'tea': 'Assam'}"_json);
    test_data.set("tmp.zip"_key, content);
    test_data.save();

    test_data.reset();
    EXPECT_EQ(test_data["['tmp.zip'].tmp['some.json'].tea"_path], "Assam");
}
