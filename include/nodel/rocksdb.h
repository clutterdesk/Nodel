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

#include <nodel/rocksdb/DB.h>
#include <nodel/filesystem.h>
#include <nodel/support/logging.h>

using namespace nodel;

namespace nodel::rocksdb {

/**
 * @brief Register a directory extension to recognize RocksDB database directories.
 * @param fobj A filesystem object.
 * @param ext The directory extension, including the leading period.
 * @note The extension is registered for the entire tree.
 */
inline
void register_directory_extension(const Object& fobj, const std::string& ext = ".rocksdb") {
    Ref<Registry> r_reg = filesystem::get_registry(dir);
    if (!r_reg) {
        WARN("Argument must be a filesystem directory object.");
        return;
    }

    r_reg->add_directory(ext, [] (const Object& target, const std::filesystem::path& path) {
        return new rocksdb::DB(DataSource::Origin::SOURCE);
    });
}

/**
 * @brief Register a directory name pattern to recognize RocksDB database directories.
 * @param fobj A filesystem object.
 * @param ext The directory extension, including the leading period.
 * @note The pattern is registered for the entire tree.
 */
inline
void register_directory_pattern(const Object& dir, const std::regex& pattern) {
    Ref<Registry> r_reg = filesystem::get_registry(dir);
    if (!r_reg) {
        WARN("Argument must be a filesystem directory object.");
        return;
    }

    r_reg->add_directory(pattern, [] (const Object& target, const std::filesystem::path& path) {
        return new rocksdb::DB(DataSource::Origin::SOURCE);
    });
}

} // namespace nodel::rocksdb

