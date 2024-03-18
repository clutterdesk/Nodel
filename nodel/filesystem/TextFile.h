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
#include <nodel/impl/Object.h>

#include <fstream>

namespace nodel {
namespace filesystem {

class TextFile : public File
{
  public:
    TextFile(const std::string& ext, Origin origin) : File(ext, Mode::INHERIT, Object::STR_I, origin) {}
    TextFile(const std::string& ext)                : TextFile(ext, Origin::MEMORY) {}
    TextFile()                                      : TextFile(".txt") {}

    DataSource* copy(const Object& target, Origin origin) const override { return new TextFile(ext(), origin); }

    void read(const Object& target, Object& cache) override;
    void write(const Object& target, const Object& cache, bool quiet) override;
};

inline
void TextFile::read(const Object& target, Object& cache) {
    auto fpath = path(target).string();
    auto size = std::filesystem::file_size(fpath);
    std::ifstream f_in{fpath, std::ios::in | std::ios::binary};
    cache = "";
    auto& str = cache.as<String>();
    str.resize(size);
    f_in.read(str.data(), size);
    if (f_in.fail() || f_in.bad())  set_failed(true);
}

inline
void TextFile::write(const Object& target, const Object& cache, bool quiet) {
    auto fpath = path(target).string();
    auto& str = cache.as<String>();
    std::ofstream f_out{fpath + ext(), std::ios::out};
    f_out.exceptions(std::ifstream::failbit);
    f_out.write(&str[0], str.size());
    if (f_out.fail() || f_out.bad()) set_failed(true);
    // TODO: quiet
}

} // namespace filesystem
} // namespace nodel

