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
// limitations under the License.#pragma once
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

struct DirectoryNotFound : std::exception
{
    DirectoryNotFound() = default;
    const char* what() const noexcept override { return "Directory not found"; }
};

namespace impl {

// DataSource for filesystem directories
//
class SubDirectory : public DataSource
{
  public:
    SubDirectory(int mode=READ|WRITE) : DataSource(Sparse::COMPLETE, mode) {
        m_cache = Object{Object::OMAP_I};
    }

    DataSource* new_instance(const Object& target) const override { return new SubDirectory(m_mode); }

    void read_meta(const Object&, Object&) override { assert (false); } // TODO: remove, later
    void read(const Object& target, Object& cache) override;
    void write(const Object& target, const Object& cache) override;
};

} // namespace impl


// DataSource to use for filesystem directory
// Example:
//     Object o = Directory(<path>);
//
class Directory : public impl::SubDirectory
{
  public:
    Directory(Ref<Registry> r_registry, const std::filesystem::path& path, int mode=READ|WRITE)
      : SubDirectory(mode) , mr_registry(r_registry) , m_path(path) {}

    DataSource* new_instance(const Object& target) const override { return new Directory(mr_registry, m_path, m_mode); }

    Ref<Registry> registry() const { return mr_registry; }
    std::filesystem::path path() const { return m_path; }

  private:
    Ref<Registry> mr_registry;
    std::filesystem::path m_path;
};

inline
bool is_dir(const Object& obj) {
    return typeid(obj.data_source<DataSource>()) == typeid(Directory);
}

inline
bool is_file(const Object& obj) {
    return typeid(obj.data_source<DataSource>()) == typeid(File);
}

inline
bool is_fs(const Object& obj) {
    auto& obj_typeid = typeid(obj.data_source<DataSource>());
    return obj_typeid == typeid(Directory) || obj_typeid == typeid(File);
}

inline
bool is_fs_head(const Object& obj) {
    return obj.data_source<Directory>() != nullptr;
}

inline
Object find_fs_head(const Object& target) {
    return find_first(target.iter_lineage(), is_fs_head);
}

inline
std::filesystem::path path(const Object& obj) {
    auto head_anc = find_fs_head(obj);
    if (head_anc.is_empty()) throw DirectoryNotFound();

    auto p_dsrc = head_anc.data_source<Directory>();
    std::filesystem::path fpath = p_dsrc->path();

    for (auto& key : obj.path(head_anc.parent()))
        fpath /= key.to_str();

    return fpath;
}

inline
void impl::SubDirectory::read(const Object& target, Object& cache) {
    auto head_anc = find_fs_head(target);
    if (head_anc.is_empty()) throw DirectoryNotFound();

    auto opath = target.path(head_anc.parent());
    auto fpath = path(target);

    for (const auto& entry : std::filesystem::directory_iterator(fpath)) {
        auto fname = entry.path().filename().string();
        if (entry.is_directory()) {
            cache.set(fname, new impl::SubDirectory(m_mode));
        } else {
            auto& head_ds = *(head_anc.data_source<Directory>());
            auto r_reg = head_ds.registry();
            auto p_ds = r_reg->new_file(target, entry.path());
            if (p_ds != nullptr) {
                p_ds->set_mode(m_mode);
                // TODO: support key customization
//                auto key = p_ds->make_key(target);
//                cache[p_ds->make_key(target)] = p_ds;
                cache.set(fname, p_ds);
            }
        }
    }
}

inline
void impl::SubDirectory::write(const Object& target, const Object& cache) {
    auto fpath = path(target);
    fmt::print("Overwrite not implemented, yet: {}\n", fpath.string());
}

} // namespace filesystem
} // namespace nodel
