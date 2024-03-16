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

#include <nodel/impl/Object.h>
#include <filesystem>

namespace nodel {
namespace filesystem {

class File : public DataSource
{
  public:
    File(const std::string& ext, Origin origin)                   : DataSource(Kind::COMPLETE, Mode::READ, origin), m_ext{ext} {}
    File(const std::string& ext, ReprType repr_ix, Origin origin) : DataSource(Kind::COMPLETE, Mode::READ, repr_ix, origin), m_ext{ext} {}

    std::string ext() const { return m_ext; }

  private:
    std::string m_ext;
};

} // namespace filesystem
} // namespace nodel

