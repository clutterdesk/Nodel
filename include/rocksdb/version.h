// Copyright (C) 2023 Speedb Ltd. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Copyright (c) 2011-present, Facebook, Inc.  All rights reserved.
//  This source code is licensed under both the GPLv2 (found in the
//  COPYING file in the root directory) and Apache 2.0 License
//  (found in the LICENSE.Apache file in the root directory).
#pragma once

#include <string>
#include <unordered_map>

#include "rocksdb/rocksdb_namespace.h"

// NOTE: in 'main' development branch, this should be the *next*
// minor or major version number planned for release.
#define ROCKSDB_MAJOR 8
#define ROCKSDB_MINOR 6
#define ROCKSDB_PATCH 7

// Do not use these. We made the mistake of declaring macros starting with
// double underscore. Now we have to live with our choice. We'll deprecate these
// at some point
#define __ROCKSDB_MAJOR__ ROCKSDB_MAJOR
#define __ROCKSDB_MINOR__ ROCKSDB_MINOR
#define __ROCKSDB_PATCH__ ROCKSDB_PATCH

namespace ROCKSDB_NAMESPACE {
// Returns a set of properties indicating how/when/where this version of RocksDB
// was created.
const std::unordered_map<std::string, std::string>& GetRocksBuildProperties();

// Returns a set of debug properties such as PORTABLE, DEBUG_LEVEL
// and USE_RTTI indicating how was created.
const std::unordered_map<std::string, std::string>& GetRocksDebugProperties();

// Returns the current version of RocksDB as a string (e.g. "6.16.0").
// If with_patch is true, the patch is included (6.16.x).
// Otherwise, only major and minor version is included (6.16)
std::string GetRocksVersionAsString(bool with_patch = true);

// Gets the set of build properties (@see GetRocksBuildProperties) into a
// string. Properties are returned one-per-line, with the first line being:
// "<program> from RocksDB <version>.
// If verbose is true, the full set of properties is
// printed. If verbose is false, only the version information (@see
// GetRocksVersionString) is printed.
std::string GetRocksBuildInfoAsString(const std::string& program,
                                      bool verbose = false);
//// Gets the set of build properties (@see GetRocksBuildProperties) into a
// string. Properties are returned one-per-line, with the first line being:
// "<program> from RocksDB <version>.
// If verbose is true, the full set of properties is
// printed. If verbose is false, only the version information (@see
// GetRocksVersionString) is printed.
std::string GetRocksBuildFlagsAsString();
//// Gets the set of build debug properties (@see GetRocksDebugProperties())
// into a string.
// Properties are returned on after another(if defined) in a single line.
std::string GetRocksDebugPropertiesAsString();
}  // namespace ROCKSDB_NAMESPACE
