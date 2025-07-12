/// @file
/// @b License: @n Apache License v2.0
/// @copyright Robert Dunnagan
#include <gtest/gtest.h>
#include <fmt/format.h>
#include <nodel/core.hxx>
#include <filesystem>

#include <nodel/support/Finally.hxx>
#include <nodel/filesystem.hxx>

using namespace nodel;
using namespace nodel::filesystem;


struct TestFilesystemAlien : public Alien
{
    TestFilesystemAlien(const std::string& data) : buf{data} {}

    std::unique_ptr<Alien> clone() override { return std::make_unique<TestFilesystemAlien>(buf); }
    String to_str() const override { return buf; }
    String to_json(int indent) const override { return '"' + buf + '"'; }

    std::string buf;
};


TEST(Filesystem, IsFsobj) {
    auto path = std::filesystem::current_path() / "test_data";
    Object test_data = new Directory(new Registry{default_registry()}, path, DataSource::Origin::SOURCE);
    EXPECT_TRUE(is_fs(test_data));
    EXPECT_TRUE(is_fs(test_data.get("more"_key)));
    EXPECT_TRUE(is_fs(test_data.get("example.json"_key)));
    EXPECT_TRUE(is_fs(test_data.get("example.txt"_key)));
    EXPECT_TRUE(is_fs(test_data.get("more"_key).get("example.csv"_key)));
}

TEST(Filesystem, IsDir) {
    auto path = std::filesystem::current_path() / "test_data";
    Object test_data = new Directory(new Registry{default_registry()}, path, DataSource::Origin::SOURCE);
    EXPECT_TRUE(is_dir(test_data));
    EXPECT_TRUE(is_dir(test_data.get("more"_key)));
    EXPECT_FALSE(is_dir(test_data.get("example.json"_key)));
    EXPECT_FALSE(is_dir(test_data.get("example.txt"_key)));
    EXPECT_FALSE(is_dir(test_data.get("more"_key).get("example.csv"_key)));
}

TEST(Filesystem, IsFile) {
    auto path = std::filesystem::current_path() / "test_data";
    Object test_data = new Directory(new Registry{default_registry()}, path, DataSource::Origin::SOURCE);
    EXPECT_FALSE(is_file(test_data));
    EXPECT_FALSE(is_file(test_data.get("more"_key)));
    EXPECT_TRUE(is_file(test_data.get("example.json"_key)));
    EXPECT_TRUE(is_file(test_data.get("example.txt"_key)));
    EXPECT_TRUE(is_file(test_data.get("more"_key).get("example.csv"_key)));
}

TEST(Filesystem, PathOfBoundObject) {
    auto path = std::filesystem::current_path() / "test_data";
    Object obj = new Directory(new Registry{default_registry()}, path, DataSource::Origin::SOURCE);
    EXPECT_EQ(filesystem::path(obj), path);
}

TEST(Filesystem, PathOfUnboundObject) {
    Object obj = "{}"_json;
    EXPECT_TRUE(filesystem::path(obj).empty());
}

TEST(Filesystem, VisitOnlyFiles) {
    auto path = std::filesystem::current_path() / "test_data";
    Object test_data = new Directory(new Registry{default_registry()}, path, DataSource::Origin::SOURCE);
    ObjectList found;
    for (const auto& file : test_data.iter_tree(is_file)) {
        EXPECT_TRUE(is_file(file));
    }
}

TEST(Filesystem, EnterOnlyDirectories) {
    auto path = std::filesystem::current_path() / "test_data";
    Object test_data = new Directory(new Registry{default_registry()}, path, DataSource::Origin::SOURCE);
    Object more = test_data.get("more"_key);
    for (const auto& file : test_data.iter_tree_if(is_dir)) {
        Object parent = file.parent();
        EXPECT_TRUE(parent == nil || parent.is(test_data) || parent.is(more));
    }
}

TEST(Filesystem, Directory) {
    auto path = std::filesystem::current_path() / "test_data";
    Object obj = new Directory(new Registry{default_registry()}, path, DataSource::Origin::SOURCE);
    EXPECT_TRUE(nodel::is_map(obj));
    EXPECT_FALSE(obj.data_source<DataSource>()->is_fully_cached());
    EXPECT_TRUE(obj.size() > 0);
}

TEST(Filesystem, DirectoryFiles) {
    auto path = std::filesystem::current_path() / "test_data";
    Object obj = new Directory(new Registry{default_registry()}, path, DataSource::Origin::SOURCE);
    EXPECT_TRUE(nodel::is_map(obj));
    EXPECT_FALSE(obj.data_source<DataSource>()->is_fully_cached());
    EXPECT_TRUE(obj.size() > 0);

    auto example_csv = obj.get("example.csv"_key);
    EXPECT_TRUE(example_csv.parent().is(obj));
    EXPECT_TRUE(!example_csv.data_source<DataSource>()->is_fully_cached());
    EXPECT_TRUE(example_csv.is_type<ObjectList>());
    EXPECT_TRUE(example_csv.get(7).is_type<ObjectList>());
    EXPECT_EQ(example_csv.get(7).get(2), "Peg");

    auto example_txt = obj.get("example.txt"_key);
    EXPECT_TRUE(example_txt.parent().is(obj));
    EXPECT_TRUE(!example_txt.data_source<DataSource>()->is_fully_cached());
    EXPECT_TRUE(example_txt.is_type<String>());
    EXPECT_TRUE(example_txt.as<String>().find("boring") != std::string::npos);

    auto example = obj.get("example.json"_key);
    EXPECT_TRUE(example.parent().is(obj));
    EXPECT_TRUE(!example.data_source<DataSource>()->is_fully_cached());
    EXPECT_TRUE(example.get("teas"_key).is_type<ObjectList>());
    EXPECT_EQ(example.get("teas"_key).get(0), "Assam");

    auto large_example_1 = obj.get("large_example_1.json"_key);
    EXPECT_FALSE(large_example_1.data_source<DataSource>()->is_fully_cached());
    EXPECT_EQ(large_example_1.get(0).get("guid"_key), "20e19d58-d42c-4bb9-a370-204de2fc87df");
    EXPECT_TRUE(large_example_1.data_source<DataSource>()->is_fully_cached());

    auto large_example_2 = obj.get("large_example_2.json"_key);
    EXPECT_FALSE(large_example_2.data_source<DataSource>()->is_fully_cached());
    EXPECT_EQ(large_example_2.get("result"_key).get(-1).get("location"_key).get("city"_key), "Indianapolis");
    EXPECT_TRUE(large_example_2.data_source<DataSource>()->is_fully_cached());
}

TEST(Filesystem, Subdirectory) {
    auto path = std::filesystem::current_path() / "test_data";
    Object test_data = new Directory(new Registry{default_registry()}, path, DataSource::Origin::SOURCE);
    EXPECT_TRUE(nodel::is_map(test_data.get("more"_key)));
    EXPECT_TRUE(test_data.get("more"_key).get("example.csv"_key).is_type<ObjectList>());
    EXPECT_TRUE(test_data.get("more"_key).get("example.csv"_key).get(-1).is_type<ObjectList>());
    EXPECT_EQ(test_data.get("more"_key).get("example.csv"_key).get(-1).get(-1), "andrew43514@gmail.comField Tags");
}

TEST(Filesystem, CreateDirectory) {
    std::string temp_dir_name = "temp_test_create";
    auto path = std::filesystem::current_path() / "test_data";

    Finally cleanup{ [&path, &temp_dir_name] () { std::filesystem::remove(path / temp_dir_name); } };

    Object test_data = new Directory(new Registry{default_registry()}, path, DataSource::Origin::SOURCE);
    test_data.set(temp_dir_name, new SubDirectory(DataSource::Origin::MEMORY));
    test_data.save();

    Object test_data_2 = new Directory(new Registry{default_registry()}, path, DataSource::Origin::SOURCE);
    EXPECT_TRUE(test_data_2.get(temp_dir_name) != nil);

    test_data.reset();
    EXPECT_TRUE(test_data.get(temp_dir_name) != nil);
}

TEST(Filesystem, DeleteDirectory) {
    std::string temp_dir_name = "temp_test_delete";
    using Mode = DataSource::Mode;
    auto path = std::filesystem::current_path() / "test_data";

    Finally cleanup{ [&path, &temp_dir_name] () { std::filesystem::remove(path / temp_dir_name); } };

    auto p_ds = new Directory(new Registry{default_registry()}, path, DataSource::Origin::SOURCE);
    p_ds->set_options({Mode::ALL});
    Object test_data = p_ds;
    test_data.set(temp_dir_name, new SubDirectory(DataSource::Origin::MEMORY));
    test_data.save();
    EXPECT_TRUE(std::filesystem::exists(path / temp_dir_name));

    p_ds = new Directory(new Registry{default_registry()}, path, DataSource::Origin::SOURCE);
    p_ds->set_options({Mode::ALL});
    Object test_data_2 = p_ds;
    EXPECT_TRUE(test_data_2.get(temp_dir_name) != nil);
    test_data_2.del(temp_dir_name);

    test_data_2.save();
    EXPECT_FALSE(std::filesystem::exists(path / temp_dir_name));

    test_data.reset();
    EXPECT_TRUE(test_data.get(temp_dir_name) == nil);
}

TEST(Filesystem, CreateJsonFile) {
    std::string new_file_name = "new_file.json";
    auto path = std::filesystem::current_path() / "test_data";

    Finally cleanup{ [&path, &new_file_name] () { std::filesystem::remove(path / new_file_name); } };

    Object test_data = new Directory(new Registry{default_registry()}, path, DataSource::Origin::SOURCE);
    Object new_file = new SerialFile(new JsonSerializer());
    new_file.set(json::parse("{'tea': 'Assam, please'}"));
    test_data.set(new_file_name, new_file);
    test_data.save();

    Object test_data_2 = new Directory(new Registry{default_registry()}, path, DataSource::Origin::SOURCE);
    EXPECT_EQ(test_data_2.get(new_file_name).get("tea"_key), "Assam, please");

    test_data.reset();
    EXPECT_EQ(test_data.get(new_file_name).get("tea"_key), "Assam, please");
}

TEST(Filesystem, CreateIndentedJsonFile) {
    std::string new_file_name = "new_file.json";
    auto path = std::filesystem::current_path() / "test_data";

    Finally cleanup{ [&path, &new_file_name] () { std::filesystem::remove(path / new_file_name); } };

    Object test_data = new Directory(new Registry{default_registry()}, path, DataSource::Origin::SOURCE);
    Object new_file = new SerialFile(new JsonSerializer());
    new_file.set(json::parse("{'tea': 'Assam, please'}"));
    test_data.set(new_file_name, new_file);
    test_data.save("{'indent': 1}"_json);

    auto json_path = path / new_file_name;
    std::ifstream file_stream(json_path);
    std::string content((std::istreambuf_iterator<char>(file_stream)), std::istreambuf_iterator<char>());
    EXPECT_EQ(content, "{\n \"tea\": \"Assam, please\"\n}");
}

TEST(Filesystem, CreateCsvFile) {
    std::string new_file_name = "new_file.csv";
    auto path = std::filesystem::current_path() / "test_data";

    Finally cleanup{ [&path, &new_file_name] () { std::filesystem::remove(path / new_file_name); } };

    Object test_data = new Directory(new Registry{default_registry()}, path, DataSource::Origin::SOURCE);
    Object new_file = new SerialFile(new CsvSerializer());
    new_file.set(0, json::parse("['a', 'b']"));
    new_file.set(1, json::parse("[0, 1]"));
    new_file.set(2, json::parse("[2, 3]"));
    test_data.set(new_file_name, new_file);
    test_data.save();

    Object test_data_2 = new Directory(new Registry{default_registry()}, path, DataSource::Origin::SOURCE);
    auto csv = test_data_2.get(new_file_name);
    EXPECT_EQ(csv.get(0).to_json(), R"(["a", "b"])");
    EXPECT_EQ(csv.get(1).to_json(), R"([0, 1])");
    EXPECT_EQ(csv.get(2).to_json(), R"([2, 3])");
}

TEST(Filesystem, UpdateJsonFile) {
    std::string new_file_name = "new_file.json";
    auto path = std::filesystem::current_path() / "test_data";

    Finally cleanup{ [&path, &new_file_name] () { std::filesystem::remove(path / new_file_name); } };

    Object test_data = new Directory(new Registry{default_registry()}, path, DataSource::Origin::SOURCE);
    Object new_file = new SerialFile(new JsonSerializer());
    new_file.set(json::parse("{'tea': 'Assam, please'}"));
    test_data.set(new_file_name, new_file);
    test_data.save();

    test_data.reset();
    new_file = test_data.get(new_file_name);
    new_file.set("tea"_key, "Assam, thanks!");
    test_data.save();

    test_data.reset();
    EXPECT_EQ(test_data.get(new_file_name).get("tea"_key), "Assam, thanks!");
}

TEST(Filesystem, SaveNewDeep1) {
    auto path = std::filesystem::current_path() / "test_data";

    Finally cleanup{ [&path] () { std::filesystem::remove_all(path / "tmp"); } };

    Object test_data = new Directory(new Registry{default_registry()}, path, DataSource::Origin::SOURCE);
    test_data.set("tmp"_key, json::parse("{'tea.txt': 'FTGFOP'}"));
    EXPECT_EQ(test_data.get("tmp['tea.txt']"_path).path().to_str(), "tmp[\"tea.txt\"]");

    test_data.save();

    Object test_data_2 = new Directory(new Registry{default_registry()}, path, DataSource::Origin::SOURCE);
    EXPECT_EQ(test_data_2.get("tmp"_key).get("tea.txt"_key), "FTGFOP");

    test_data.reset();
    EXPECT_EQ(test_data.get("tmp"_key).get("tea.txt"_key), "FTGFOP");
}

TEST(Filesystem, SaveNewDeep2) {
    auto path = std::filesystem::current_path() / "test_data";

    Finally cleanup{ [&path] () { std::filesystem::remove_all(path / "tmp"); } };

    Object test_data = nodel::bind(fmt::format("file://?path={}&perm=rw", path.string()));
    test_data.set("tmp"_key, json::parse("{}"));
    test_data.get("tmp"_key).set("tea.txt"_key, "FTGFOP");

    test_data.save();

    Object test_data_2 = nodel::bind(fmt::format("file://?path={}&perm=r", path.string()));
    EXPECT_EQ(test_data_2.get("tmp"_key).get("tea.txt"_key), "FTGFOP");

    test_data.reset();
    EXPECT_EQ(test_data.get("tmp"_key).get("tea.txt"_key), "FTGFOP");
}

TEST(Filesystem, SaveNewDeeper) {
    auto path = std::filesystem::current_path() / "test_data";

    Finally cleanup{ [&path] () { std::filesystem::remove_all(path / "tmp"); } };

    Object test_data = nodel::bind(fmt::format("file://?path={}&perm=rw", path.string()));
    test_data.set("tmp"_key, json::parse("{'dir1': {'tea.txt': 'FTGFOP'}, 'dir2': {'tea.txt': 'SFTGFOP'}}"));
    test_data.save();

    Object test_data_2 = nodel::bind(fmt::format("file://?path={}&perm=r", path.string()));
    EXPECT_EQ(test_data_2.get("tmp.dir1['tea.txt']"_path), "FTGFOP");
    EXPECT_EQ(test_data_2.get("tmp.dir2['tea.txt']"_path), "SFTGFOP");

    test_data.reset();
    EXPECT_EQ(test_data.get("tmp.dir1['tea.txt']"_path), "FTGFOP");
    EXPECT_EQ(test_data.get("tmp.dir2['tea.txt']"_path), "SFTGFOP");
}

TEST(Filesystem, CopyFileToAnotherDirectory) {
    auto path = std::filesystem::current_path() / "test_data";

    Finally cleanup{[&path] () {
        std::filesystem::remove(path / "temp" / "example.json");
        std::filesystem::remove(path / "temp");
    }};

    Object test_data = new Directory(new Registry{default_registry()}, path, DataSource::Origin::SOURCE);
    test_data.set("temp"_key, new SubDirectory(DataSource::Origin::MEMORY));
    test_data.save();

    Object test_data_2 = new Directory(new Registry{default_registry()}, path, DataSource::Origin::SOURCE);
    auto temp = test_data_2.get("temp"_key);
    temp.set("example.json"_key, test_data.get("example.json"_key));
    EXPECT_TRUE(temp.get("example.json"_key).parent().is(temp));
    test_data_2.save();

    test_data.reset();
    temp = test_data.get("temp"_key);
    EXPECT_FALSE(temp == nil);
    EXPECT_TRUE(temp.get("example.json"_key) != nil);
    EXPECT_EQ(temp.get("example.json"_key).get("favorite"_key), "Assam");
}

TEST(Filesystem, Bind) {
    Object path = bind("file://?path=."_uri);
    EXPECT_TRUE(path.get("test_data"_key) != nil);
}

TEST(Filesystem, LookupDataSourceForNewFile) {
    Object wd = bind("file://?perm=rw&path=."_uri);
    auto path = filesystem::path(wd);

    Finally cleanup{[&path] () {
        std::filesystem::remove(path / "test_data" / "dummy.txt");
    }};

    wd["test_data['dummy.txt']"_path] = "tea";
    wd.save();

    wd.reset();
    EXPECT_EQ(wd["test_data['dummy.txt']"_path], "tea");
}

TEST(Filesystem, ResolveDataSource) {
    Object wd = bind("file://?perm=rw&path=."_uri);
    auto fpath = filesystem::path(wd);

    Finally cleanup{[&fpath] () {
        std::filesystem::remove(fpath / "test_data" / "foo" / "dummy.txt");
        std::filesystem::remove(fpath / "test_data" / "foo");
    }};

    OPath path = "test_data.foo['dummy.txt']"_path;
    auto dummy = path.create(wd, "tea");
    EXPECT_EQ(dummy.key(), "dummy.txt");
    EXPECT_TRUE(has_data_source(wd["test_data.foo"_path]));
    EXPECT_TRUE(has_data_source(dummy));
    EXPECT_EQ(dummy.path().to_str(), "test_data.foo[\"dummy.txt\"]");

    wd.save();
    EXPECT_EQ(filesystem::path(dummy).string(), "./test_data/foo/dummy.txt");

    wd.reset();
    EXPECT_EQ(wd.get(path), "tea");
}

void test_invalid_file(const std::string& file_name) {
    auto path = std::filesystem::current_path() / "test_data";
    DataSource::Options options;
    options.throw_read_error = false;
    options.throw_write_error = false;
    auto p_ds = new Directory(new Registry{default_registry()}, path, DataSource::Origin::SOURCE);
    p_ds->set_options(options);
    Object test_data = p_ds;
    Object fs_obj = test_data.get(file_name);
    EXPECT_TRUE(fs_obj.is_valid());

    auto fs_obj_path = nodel::filesystem::path(fs_obj);
    std::filesystem::permissions(fs_obj_path, std::filesystem::perms::none);

    Finally reset_permissions([&fs_obj_path](){
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

TEST(Filesystem, Clear) {
    Object path = bind("file://?path=test_data&perm=rw"_uri);
    EXPECT_FALSE(path.get("example.csv"_key).is_nil());
    path.clear();
    EXPECT_TRUE(path.get("example.csv"_key).is_nil());
    path.reset();
    EXPECT_FALSE(path.get("example.csv"_key).is_nil());
}

TEST(Filesystem, TooManyPathsInURI) {
    try {
        bind("file:///?path=."_uri);
        FAIL();
    } catch (NodelException e) {
    }
}

