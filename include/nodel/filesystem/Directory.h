#pragma once

#include "Registry.h"
#include "File.h"

#include <nodel/support/string.h>
#include <nodel/support/Ref.h>
#include <nodel/core/algo.h>
#include <nodel/core/Object.h>
#include <nodel/core/uri.h>

#include <filesystem>
#include <regex>

#include <fmt/core.h>

//using namespace std::ranges;

namespace nodel {
namespace filesystem {

typedef std::function<bool(const Object&)> Predicate;


/**
 * @brief DataSource for filesystem directories
 */
class SubDirectory : public DataSource
{
  public:
    SubDirectory(Options options, Origin origin) : DataSource(Kind::COMPLETE, options, Object::OMAP, origin) {}
    SubDirectory(Options options = {})           : DataSource(Kind::COMPLETE, options, Object::OMAP, Origin::MEMORY) {}
    SubDirectory(Origin origin)                  : SubDirectory({}, origin) {}

    DataSource* new_instance(const Object& target, Origin origin) const override { return new SubDirectory(options(), origin); }

    void read_type(const Object&) override { assert (false); } // TODO: remove, later
    void read(const Object& target) override;
    void write(const Object& target, const Object&) override;

    void set_registry(Ref<Registry> r_reg) { mr_registry = r_reg; }
    Ref<Registry> registry() const         { return mr_registry; }

  protected:
    Ref<Registry> mr_registry;
};


/**
 * @brief DataSource to use for filesystem directory
 * Example:
 *     Object o = Directory(<path>);
 */
class Directory : public SubDirectory
{
  public:
    Directory(Ref<Registry> r_registry, const std::filesystem::path& path, Options options = {})
      : SubDirectory(options, Origin::SOURCE), m_path(path) {
        mr_registry = r_registry;
    }

    DataSource* new_instance(const Object& target, Origin origin) const override {
        assert (origin == Origin::SOURCE);
        return new Directory(mr_registry, m_path, options());
    }

    std::filesystem::path path() const { return m_path; }

  private:
    std::filesystem::path m_path;
};


inline
bool is_dir(const Object& obj) {
    return obj.data_source<SubDirectory>() != nullptr;
}

inline
bool is_file(const Object& obj) {
    return obj.data_source<File>() != nullptr;
}

inline
bool is_fs(const Object& obj) {
    return is_dir(obj) || is_file(obj);
}

inline
bool is_fs_root(const Object& obj) {
    return obj.data_source<Directory>() != nullptr;
}

inline
Object find_fs_root(const Object& obj) {
    return algo::find_first(obj.iter_line(), is_fs_root);
}

inline
Ref<Registry> get_registry(const Object& obj) {
    auto head_anc = find_fs_root(obj);
    if (head_anc != nil) return {};
    auto& head_ds = *(head_anc.data_source<Directory>());
    return head_ds.registry();
}

inline
std::filesystem::path path(const Object& obj) {
    auto head_anc = find_fs_root(obj);
    assert (!head_anc.is_empty());

    auto p_dsrc = head_anc.data_source<Directory>();
    std::filesystem::path fpath = p_dsrc->path();

    for (auto& key : obj.path(head_anc.parent()))
        fpath /= key.to_str();

    return fpath;
}

// TODO: move to predicate.h?
struct RegexFilter
{
    RegexFilter(String&& regex) : m_regex{std::forward<String>(regex)} {}
    bool operator () (const Object& obj) {
        return std::regex_match(path(obj).filename().string(), m_regex);
    }
    std::regex m_regex;
};

inline
Predicate make_regex_filter(String&& regex) {
    return RegexFilter{std::forward<String>(regex)};
}


inline
void SubDirectory::read(const Object& target) {
    auto head_anc = find_fs_root(target);
    assert (head_anc != nil);
    auto opath = target.path(head_anc.parent());
    auto fpath = path(target);
    // TODO: handle directory_iterator failure
    for (const auto& entry : std::filesystem::directory_iterator(fpath)) {
        auto fname = entry.path().filename().string();
        auto& head_ds = *(head_anc.data_source<Directory>());
        auto r_reg = head_ds.registry();
        auto p_ds = r_reg->create(target, entry.path(), entry.is_directory());
        if (p_ds != nullptr) {
            p_ds->set_options(options());
            read_set(target, fname, p_ds);
        }
    }
}

inline
void SubDirectory::write(const Object& target, const Object& cache) {
    auto head_anc = find_fs_root(target);
    auto opath = target.path(head_anc.parent());
    auto fpath = path(target);

    // create, if necessary
    if (!std::filesystem::exists(fpath)) {
//        DEBUG("Creating directory: {}", fpath.string());
        std::filesystem::create_directory(fpath);
    }

    // find deleted files and directories
    std::vector<std::filesystem::path> deleted;
    for (const auto& entry : std::filesystem::directory_iterator(fpath)) {
        auto fname = entry.path().filename().string();
        if (cache.get(fname) == nil)
            deleted.push_back(entry.path());
    }

    // perform deletes
    std::vector<std::filesystem::path> failed_deletes;
    for (auto& deleted : deleted) {
//        DEBUG("Removing directory: {}", deleted.string());
        std::error_code err;
        std::filesystem::remove_all(deleted, err);
        if (err) report_write_error(err.message());
    }
}

} // namespace filesystem
} // namespace nodel
