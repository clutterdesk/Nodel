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

#include <nodel/support/Ref.h>
#include <nodel/core/Object.h>

#include <filesystem>
#include <functional>

namespace nodel::filesystem {

/**
 * @brief A registry that maps filesystem patterns and extensions to DataSources.
 * - Registry entries may be defined for a regular expression pattern, or a file extension.
 * - In general the schema of a file cannot be determined by examining its content. The Registry
 *   provides a means of determining schema from the filesystem path of the file.
 * - If no rule applies for a given file, or directory, the default factory is used, if present.
 *   Separate default factories are provided for files and directories.
 * - If no rule applies, and no default factory is defined, no object will be created to represent
 *   the path. Such elided files/directories will not be removed when the parent object is saved.
 * @see nodel::filesystem::DefaultFactory
 */
class Registry
{
  public:
    using Origin = DataSource::Origin;
    using Factory = std::function<DataSource*(const Object&, const std::filesystem::path&)>;

    void set_file_default(const Factory& factory);
    void set_directory_default(const Factory& factory);
    void associate(const String& ext, const Factory& factory);

    template <class T> void set_file_default();
    template <class T> void set_directory_default();
    template <class T> void associate(const String& extension);

    DataSource* create(const Object& target, const std::filesystem::path& path, bool is_directory) const;

    bool is_empty() const;

  protected:
    std::unordered_map<String, Factory> m_ext_map;
    Factory m_dir_default;
    Factory m_file_default;

  private:
    refcnt_t m_ref_count;

  template <typename> friend class ::nodel::Ref;
};


inline
void Registry::set_file_default(const Factory& factory) {
    m_file_default = factory;
}

inline
void Registry::set_directory_default(const Factory& factory) {
    m_dir_default = factory;
}

inline
void Registry::associate(const String& ext, const Factory& factory) {
    m_ext_map[ext] = factory;
}

template <class T>
void Registry::set_file_default() {
    m_file_default = [] (const Object&, const std::filesystem::path&) { return new T(Origin::SOURCE); };
}

template <class T>
void Registry::set_directory_default() {
    m_dir_default = [] (const Object&, const std::filesystem::path&) { return new T(Origin::SOURCE); };
}

template <class T>
void Registry::associate(const String& ext) {
    m_ext_map[ext] = [] (const Object&, const std::filesystem::path&) { return new T(Origin::SOURCE); };
}

inline
DataSource* Registry::create(const Object& target, const std::filesystem::path& path, bool is_directory) const {
    auto ext = path.extension().string();
    if (auto it = m_ext_map.find(ext); it != m_ext_map.end()) {
        return it->second(target, path);
    }
    return is_directory? m_dir_default(target, path): m_file_default(target, path);
}

inline
bool Registry::is_empty() const {
    return !m_file_default && !m_dir_default && m_ext_map.size() == 0;
}

} // namespace nodel::filesystem
