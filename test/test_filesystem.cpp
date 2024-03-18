#include <gtest/gtest.h>
#include <fmt/format.h>
#include <filesystem>

#include <nodel/filesystem/Directory.h>
#include <nodel/filesystem/DefaultRegistry.h>
#include <nodel/filesystem/JsonFile.h>
#include <nodel/nodel.h>

using namespace nodel;
using namespace nodel::filesystem;

TEST(Filesystem, IsFsobj) {
    auto wd = std::filesystem::current_path() / "test_data";
    Object test_data = new Directory(new DefaultRegistry(), wd);
    EXPECT_TRUE(is_fs(test_data));
    EXPECT_TRUE(is_fs(test_data.get("more")));
    EXPECT_TRUE(is_fs(test_data.get("example.json")));
    EXPECT_TRUE(is_fs(test_data.get("example.txt")));
    EXPECT_TRUE(is_fs(test_data.get("more").get("example.csv")));
}

TEST(Filesystem, IsDir) {
    auto wd = std::filesystem::current_path() / "test_data";
    Object test_data = new Directory(new DefaultRegistry(), wd);
    EXPECT_TRUE(is_dir(test_data));
    EXPECT_TRUE(is_dir(test_data.get("more")));
    EXPECT_FALSE(is_dir(test_data.get("example.json")));
    EXPECT_FALSE(is_dir(test_data.get("example.txt")));
    EXPECT_FALSE(is_dir(test_data.get("more").get("example.csv")));
}

TEST(Filesystem, IsFile) {
    auto wd = std::filesystem::current_path() / "test_data";
    Object test_data = new Directory(new DefaultRegistry(), wd);
    EXPECT_FALSE(is_file(test_data));
    EXPECT_FALSE(is_file(test_data.get("more")));
    EXPECT_TRUE(is_file(test_data.get("example.json")));
    EXPECT_TRUE(is_file(test_data.get("example.txt")));
    EXPECT_TRUE(is_file(test_data.get("more").get("example.csv")));
}

TEST(Filesystem, VisitOnlyFiles) {
    auto wd = std::filesystem::current_path() / "test_data";
    Object test_data = new Directory(new DefaultRegistry(), wd);
    List found;
    for (const auto& file : test_data.iter_tree(is_file)) {
        EXPECT_TRUE(is_file(file));
    }
}

TEST(Filesystem, EnterOnlyDirectories) {
    auto wd = std::filesystem::current_path() / "test_data";
    Object test_data = new Directory(new DefaultRegistry(), wd);
    Object more = test_data.get("more");
    for (const auto& file : test_data.iter_tree_if(is_dir)) {
        Object parent = file.parent();
        EXPECT_TRUE(parent.is_null() || parent.is(test_data) || parent.is(more));
    }
}

TEST(Filesystem, Directory) {
    auto wd = std::filesystem::current_path() / "test_data";
    Object obj = new Directory(new DefaultRegistry(), wd);
    EXPECT_TRUE(obj.is_map());
    EXPECT_FALSE(obj.data_source<DataSource>()->is_fully_cached());
    EXPECT_TRUE(obj.size() > 0);
}

TEST(Filesystem, DirectoryFiles) {
    auto wd = std::filesystem::current_path() / "test_data";
    Object obj = new Directory(new DefaultRegistry(), wd);
    EXPECT_TRUE(obj.is_map());
    EXPECT_FALSE(obj.data_source<DataSource>()->is_fully_cached());
    EXPECT_TRUE(obj.size() > 0);

    auto example_csv = obj.get("example.csv");
    EXPECT_TRUE(example_csv.parent().is(obj));
    EXPECT_TRUE(!example_csv.data_source<DataSource>()->is_fully_cached());
    EXPECT_TRUE(example_csv.is_list());
    EXPECT_TRUE(example_csv.get(7).is_list());
    EXPECT_EQ(example_csv.get(7).get(2), "Peg");

    auto example_txt = obj.get("example.txt");
    EXPECT_TRUE(example_txt.parent().is(obj));
    EXPECT_TRUE(!example_txt.data_source<DataSource>()->is_fully_cached());
    EXPECT_TRUE(example_txt.is_str());
    EXPECT_TRUE(example_txt.as<String>().contains("boring"));

    auto example = obj.get("example.json");
    EXPECT_TRUE(example.parent().is(obj));
    EXPECT_TRUE(!example.data_source<DataSource>()->is_fully_cached());
    EXPECT_TRUE(example.get("teas").is_list());
    EXPECT_EQ(example.get("teas").get(0), "Assam");

    auto large_example_1 = obj.get("large_example_1.json");
    EXPECT_FALSE(large_example_1.data_source<DataSource>()->is_fully_cached());
    EXPECT_EQ(large_example_1.get(0).get("guid"), "20e19d58-d42c-4bb9-a370-204de2fc87df");
    EXPECT_TRUE(large_example_1.data_source<DataSource>()->is_fully_cached());

    auto large_example_2 = obj.get("large_example_2.json");
    EXPECT_FALSE(large_example_2.data_source<DataSource>()->is_fully_cached());
    EXPECT_EQ(large_example_2.get("result").get(-1).get("location").get("city"), "Indianapolis");
    EXPECT_TRUE(large_example_2.data_source<DataSource>()->is_fully_cached());
}

//TEST(Filesystem, JsonFileWithListBug) {
//    auto wd = std::filesystem::current_path() / "test_data";
//    Object test_data = new Directory(new DefaultRegistry(), wd);
//    EXPECT_EQ(test_data.get("table.json").path()
//}

TEST(Filesystem, Subdirectory) {
    auto wd = std::filesystem::current_path() / "test_data";
    Object test_data = new Directory(new DefaultRegistry(), wd);
    EXPECT_TRUE(test_data.get("more").is_map());
    EXPECT_TRUE(test_data.get("more").get("example.csv").is_list());
    EXPECT_TRUE(test_data.get("more").get("example.csv").get(-1).is_list());
    EXPECT_EQ(test_data.get("more").get("example.csv").get(-1).get(-1), "andrew43514@gmail.comField Tags");
}

TEST(Filesystem, CreateDirectory) {
    std::string temp_dir_name = "temp_test_create";
    auto wd = std::filesystem::current_path() / "test_data";

    OnBlockExit cleanup{ [&wd, &temp_dir_name] () { std::filesystem::remove(wd / temp_dir_name); } };

    Object test_data = new Directory(new DefaultRegistry(), wd);
    test_data.set(temp_dir_name, new SubDirectory());
    test_data.save();

    Object test_data_2 = new Directory(new DefaultRegistry(), wd);
    EXPECT_FALSE(test_data_2.get(temp_dir_name).is_null());

    test_data.reset();
    EXPECT_FALSE(test_data.get(temp_dir_name).is_null());
}

TEST(Filesystem, DeleteDirectory) {
    std::string temp_dir_name = "temp_test_delete";
    using Mode = DataSource::Mode;
    auto wd = std::filesystem::current_path() / "test_data";

    OnBlockExit cleanup{ [&wd, &temp_dir_name] () { std::filesystem::remove(wd / temp_dir_name); } };

    Object test_data = new Directory(new DefaultRegistry(), wd, Mode::ALL);
    test_data.set(temp_dir_name, new SubDirectory());
    test_data.save();
    EXPECT_TRUE(std::filesystem::exists(wd / temp_dir_name));

    Object test_data_2 = new Directory(new DefaultRegistry(), wd, Mode::ALL);
    EXPECT_FALSE(test_data_2.get(temp_dir_name).is_null());
    test_data_2.del(temp_dir_name);

    test_data_2.save();
    EXPECT_FALSE(std::filesystem::exists(wd / temp_dir_name));

    test_data.reset();
    EXPECT_TRUE(test_data.get(temp_dir_name).is_null());
}

TEST(Filesystem, CreateJsonFile) {
    std::string new_file_name = "new_file.json";
    auto wd = std::filesystem::current_path() / "test_data";

    OnBlockExit cleanup{ [&wd, &new_file_name] () { std::filesystem::remove(wd / new_file_name); } };

    Object test_data = new Directory(new DefaultRegistry(), wd, DataSource::Mode::ALL);
    Object new_file = new JsonFile();
    new_file.set(parse_json("{'tea': 'Assam, please'}"));
    test_data.set(new_file_name, new_file);
    test_data.save();

    Object test_data_2 = new Directory(new DefaultRegistry(), wd);
    EXPECT_EQ(test_data_2.get(new_file_name).get("tea"), "Assam, please");

    test_data.reset();
    EXPECT_EQ(test_data.get(new_file_name).get("tea"), "Assam, please");
}

TEST(Filesystem, UpdateJsonFile) {
    std::string new_file_name = "new_file.json";
    auto wd = std::filesystem::current_path() / "test_data";

    OnBlockExit cleanup{ [&wd, &new_file_name] () { std::filesystem::remove(wd / new_file_name); } };

    Object test_data = new Directory(new DefaultRegistry(), wd, DataSource::Mode::ALL);
    Object new_file = new JsonFile();
    new_file.set(parse_json("{'tea': 'Assam, please'}"));
    test_data.set(new_file_name, new_file);
    test_data.save();

    test_data.reset();
    new_file = test_data.get(new_file_name);
    new_file.set("tea", "Assam, thanks!");
    test_data.save();

    test_data.reset();
    EXPECT_EQ(test_data.get(new_file_name).get("tea"), "Assam, thanks!");
}

void test_invalid_file(const char* file_name) {
    auto wd = std::filesystem::current_path() / "test_data";
    Object test_data = new Directory(new DefaultRegistry(), wd);
    Object fs_obj = test_data.get(file_name);
    EXPECT_TRUE(fs_obj.is_valid());

    auto fs_obj_path = nodel::filesystem::path(fs_obj);
    std::filesystem::permissions(fs_obj_path, std::filesystem::perms::none);

    OnBlockExit reset_permissions([&fs_obj_path](){
        std::filesystem::permissions(
            fs_obj_path,
            std::filesystem::perms::owner_read | std::filesystem::perms::owner_write
            | std::filesystem::perms::group_read | std::filesystem::perms::group_write
            | std::filesystem::perms::others_read
        );
    });

    fs_obj.reset();
    EXPECT_FALSE(fs_obj.is_valid());
}

TEST(Filesystem, InvalidTextFile) {
    test_invalid_file("example.txt");
}

TEST(Filesystem, InvalidCsvFile) {
    test_invalid_file("example.csv");
}

TEST(Filesystem, InvalidJsonFile) {
    test_invalid_file("example.json");
}
