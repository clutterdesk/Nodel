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

#include <nodel/filesystem/Directory.h>
#include <nodel/filesystem/DefaultRegistry.h>
#include <nodel/filesystem/JsonFile.h>
#include <nodel/filesystem/CsvFile.h>
#include <nodel/filesystem/GenericFile.h>

using namespace nodel;

namespace nodel::filesystem {

/**
 * @brief Bind the directory with the specified path using an instance of DefaultRegistry.
 * @param path A directory path. Relative paths are resolved to absolute paths immediately.
 * @param options Options passed to the Directory class.
 * @return Returns a new Object with a filesystem::Directory DataSource.
 */
inline
Object bind(const std::filesystem::path& path, DataSource::Options options = {}) {
    return new Directory(new DefaultRegistry(), path, options);
}

/**
 * @brief Bind the directory with the specified path.
 * @param r_registry The registry of file associations.
 * @param path A directory path. Relative paths are resolved to absolute paths immediately.
 * @param options Options passed to the Directory class.
 * @return Returns a new Object with a filesystem::Directory DataSource.
 */
inline
Object bind(Ref<Registry> r_registry, const std::filesystem::path& path, DataSource::Options options = {}) {
    return new Directory(r_registry, path, options);
}

} // namespace nodel::filesystem
