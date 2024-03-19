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

#include "File.h"
#include "Registry.h"

#include <nodel/impl/support.h>
#include <nodel/impl/algo.h>
#include <nodel/impl/Object.h>
#include <nodel/impl/Ref.h>

#include <filesystem>

#include <fmt/core.h>

using namespace std::ranges;

namespace nodel {
namespace filesystem {

struct FailedDeletes : std::exception
{
    FailedDeletes(const std::vector<std::filesystem::path>& paths) {
        std::stringstream ss;
        ss << "Error removing path(s): " << std::endl;
        for (auto& path : paths)
            ss << path << std::endl;
        m_msg = ss.str();
    }

    const char* what() const noexcept override { return m_msg.data(); }

    std::string m_msg;
};

// DataSource for filesystem directories
//
class SubDirectory : public DataSource
{
  public:
    SubDirectory(Mode mode = Mode::READ | Mode::WRITE) : DataSource(Kind::COMPLETE, mode, Object::OMAP_I, Origin::MEMORY) {}
    SubDirectory(Mode mode, Origin origin) : DataSource(Kind::COMPLETE, mode, Object::OMAP_I, origin) {}

    DataSource* new_instance(const Object& target, Origin origin) const override { return new SubDirectory(mode(), origin); }

    void read_type(const Object&) override { assert (false); } // TODO: remove, later
    void read(const Object& target) override;
    void write(const Object& target, const Object&, bool) override;
};


// DataSource to use for filesystem directory
// Example:
//     Object o = Directory(<path>);
//
class Directory : public SubDirectory
{
  public:
    Directory(Ref<Registry> r_registry, const std::filesystem::path& path, Mode mode = Mode::READ | Mode::WRITE)
      : SubDirectory(mode, Origin::SOURCE) , mr_registry(r_registry) , m_path(path) {}

    DataSource* new_instance(const Object& target, Origin origin) const override {
        assert (origin == Origin::SOURCE);
        return new Directory(mr_registry, m_path);
    }

    Ref<Registry> registry() const { return mr_registry; }
    std::filesystem::path path() const { return m_path; }

  private:
    Ref<Registry> mr_registry;
    std::filesystem::path m_path;
};

inline
bool is_dir(const Object& obj) {
    auto p_ds = obj.data_source<DataSource>();
    return dynamic_cast<SubDirectory*>(p_ds) != nullptr;
}

inline
bool is_file(const Object& obj) {
    auto p_ds = obj.data_source<DataSource>();
    return dynamic_cast<File*>(p_ds) != nullptr;
}

inline
bool is_fs(const Object& obj) {
    auto p_ds = obj.data_source<DataSource>();
    return dynamic_cast<SubDirectory*>(p_ds) != nullptr || dynamic_cast<File*>(p_ds) != nullptr;
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
    for (const auto& entry : std::filesystem::directory_iterator(fpath)) {
        auto fname = entry.path().filename().string();
        if (entry.is_directory()) {
            read_set(target, fname, new SubDirectory(mode(), Origin::SOURCE));
        } else {
            auto& head_ds = *(head_anc.data_source<Directory>());
            auto r_reg = head_ds.registry();
            auto p_ds = r_reg->new_file(target, entry.path());
            if (p_ds != nullptr) {
                p_ds->set_mode((mode() & Mode::WRITE)? mode() | Mode::OVERWRITE: mode());
                read_set(target, fname, p_ds);
            }
        }
    }
}

inline
void SubDirectory::write(const Object& target, const Object& cache, bool quiet) {
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
        if (cache.get(fname).is_null())
            deleted.push_back(entry.path());
    }

    // perform deletes
    std::vector<std::filesystem::path> failed_deletes;
    for (auto& deleted : deleted) {
        DEBUG("Removing directory: {}", deleted.string());
        std::error_code err;
        std::filesystem::remove_all(deleted, err);
        if (!quiet && err)
            failed_deletes.push_back(err.message());
    }

    if (!failed_deletes.empty())
        throw FailedDeletes(failed_deletes);
}

} // namespace filesystem
} // namespace nodel
