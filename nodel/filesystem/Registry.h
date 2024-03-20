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

#include <filesystem>

#include "File.h"
#include <nodel/support/Ref.h>

namespace nodel {
namespace filesystem {

namespace fs = std::filesystem;

class Registry
{
  public:
    using Factory = std::function<File*(const Object&, const fs::path&)>;

    template <typename Func>
    void set(const std::string_view& extension, Func&& factory) {
        m_ext_registry.emplace(extension, std::forward<Func>(factory));
    }

    File* new_file(const Object& target, const fs::path& path) const {
        auto ext = path.extension().string();
        if (auto it = m_ext_registry.find(ext); it != m_ext_registry.end()) {
            return it->second(target, path);
        }
        return nullptr;
    }

  protected:
    Registry() = default;

  protected:
    std::unordered_map<std::string, Factory> m_ext_registry;
    std::filesystem::path m_path;
    refcnt_t m_ref_count;

  template <typename> friend class ::nodel::Ref;
};

} // namespace filesystem
} // namespace nodel
