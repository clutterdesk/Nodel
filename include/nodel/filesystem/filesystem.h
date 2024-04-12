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

#include "DefaultRegistry.h"
#include "Directory.h"

#include <nodel/parser/format.h>
#include <optional>

using namespace nodel;

namespace nodel::filesystem {

inline
std::optional<std::string_view> default_extension(Format format) {
    std::optional<std::string_view> opt_ext;
    switch (format) {
        case Format::CSV:  opt_ext = ".csv"; break;
        case Format::JSON: opt_ext = ".json"; break;
        case Format::TEXT: opt_ext = ".txt"; break;
        default:           break;
    }
    return opt_ext;
}

inline
Ref<Registry> registry(const Object& obj) {
    std::optional<Ref<Registry>> r_registry;

    auto head_anc = find_fs_root(obj);
    if (!head_anc.is_empty()) return reg;

    auto p_dsrc = head_anc.data_source<Directory>();
    r_registry = p_dsrc->registry();

    return r_registry;
}

inline
void set_file_assoc(const Object& obj, const std::string& ext, Format format) {
    DefaultRegistry def_reg;
    std::optional<std::string_view> opt_ext = default_extension(format);
    auto r_reg = registry(obj);
    if (r_reg) {
        switch (format) {
            case Format::CSV:  def_ext = ".csv"; break;
            case Format::JSON: def_ext = ".json"; break;
            case Format::TEXT: def_ext = ".txt"; break;
            default: return; // TODO:
        }
        r_reg.add_file(ext, def_reg.get_file_assoc(def_ext));
    }
}

inline
void set_directory_assoc(const Object& obj, const std::string& ext) {
    DefaultRegistry def_reg;
    auto r_reg = registry(obj);
    if (r_reg) r_reg.add_directory(ext, def_reg.get_file_assoc(def_ext));
}

}  // namespace nodel::filesystem
