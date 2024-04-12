// Copyright 2024 Robert A. Dunnagan
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#pragma once

#include "Registry.h"

#include <nodel/support/string.h>
#include <nodel/support/Ref.h>
#include <nodel/core/algo.h>
#include <nodel/core/Object.h>

#include <filesystem>

#include <fmt/core.h>

using namespace std::ranges;

namespace nodel {
namespace filesystem {

// Base class for all files
//
class File : public DataSource
{
  public:
    File(Kind kind, Options options, Origin origin) : DataSource(kind, options, origin) {
        set_mode(mode() | Mode::INHERIT);
    }

    File(Kind kind, Options options, ReprIX repr_ix, Origin origin) : DataSource(kind, options, repr_ix, origin) {
        set_mode(mode() | Mode::INHERIT);
    }

  protected:
    void report_read_error(const std::string& path, const std::string& error);
    void report_write_error(const std::string& path, const std::string& error);
};


// DataSource for filesystem directories
//
class SubDirectory : public DataSource
{
  public:
    SubDirectory(Options options, Origin origin) : DataSource(Kind::COMPLETE, options, Object::OMAP, origin) {}
    SubDirectory(Options options = {}) : DataSource(Kind::COMPLETE, options, Object::OMAP, Origin::MEMORY) {}

    DataSource* new_instance(const Object& target, Origin origin) const override { return new SubDirectory(options(), origin); }

    void read_type(const Object&) override { assert (false); } // TODO: remove, later
    void read(const Object& target) override;
    void write(const Object& target, const Object&) override;
};


// DataSource to use for filesystem directory
// Example:
//     Object o = Directory(<path>);
//
class Directory : public SubDirectory
{
  public:
    Directory(Ref<Registry> r_registry, const std::filesystem::path& path, Options options = {})
      : SubDirectory(options, Origin::SOURCE) , mr_registry(r_registry) , m_path(path) {}

    DataSource* new_instance(const Object& target, Origin origin) const override {
        assert (origin == Origin::SOURCE);
        return new Directory(mr_registry, m_path, options());
    }

    Ref<Registry> registry() const { return mr_registry; }
    std::filesystem::path path() const { return m_path; }

  private:
    Ref<Registry> mr_registry;
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
Object find_fs_root(const Object& target) {
    return find_first(target.iter_line(), is_fs_root);
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

inline
void SubDirectory::read(const Object& target) {
    auto head_anc = find_fs_root(target);
    assert (!head_anc.is_empty());
    auto opath = target.path(head_anc.parent());
    auto fpath = path(target);
    // TODO: handle directory_iterator failure
    for (const auto& entry : std::filesystem::directory_iterator(fpath)) {
        auto fname = entry.path().filename().string();
        auto& head_ds = *(head_anc.data_source<Directory>());
        auto r_reg = head_ds.registry();
        if (entry.is_directory()) {
            auto p_ds = r_reg->new_directory(target, entry.path());
            if (p_ds != nullptr) {
                p_ds->set_options(options());
                read_set(target, fname, p_ds);
            }
        } else {
            auto p_ds = r_reg->new_file(target, entry.path());
            if (p_ds != nullptr) {
                p_ds->set_options(options());
                read_set(target, fname, p_ds);
            }
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
        DEBUG("Creating directory: {}", fpath.string());
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
        DEBUG("Removing directory: {}", deleted.string());
        std::error_code err;
        std::filesystem::remove_all(deleted, err);
        if (err) report_write_error(err.message());
    }
}

inline
void File::report_read_error(const std::string& path, const std::string& error) {
    DataSource::report_read_error(fmt::format("{} ({})", error, path));
}

inline
void File::report_write_error(const std::string& path, const std::string& error) {
    DataSource::report_write_error(fmt::format("{} ({})", error, path));
}

} // namespace filesystem
} // namespace nodel
