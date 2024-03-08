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

#include "Registry.h"
#include "JsonFile.h"
#include "CsvFile.h"
#include "TextFile.h"

namespace nodel {
namespace filesystem {

namespace fs = std::filesystem;

class DefaultRegistry : public Registry
{
  public:
    DefaultRegistry() {
        m_ext_registry[".json"] = [] (const Object&, const fs::path& path) { return new JsonFile(".json"); };
        m_ext_registry[".csv"]  = [] (const Object&, const fs::path& path) { return new CsvFile(".csv"); };
        m_ext_registry[".txt"]  = [] (const Object&, const fs::path& path) { return new TextFile(".txt"); };
    }

  template <typename> friend class ::nodel::Ref;
};

} // namespace filesystem
} // namespace nodel
