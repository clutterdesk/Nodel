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

#include <nodel/core/Object.h>

#include <fstream>

namespace nodel {
namespace filesystem {

class GenericFile : public File
{
  public:
    GenericFile(Options options, Origin origin)   : File(Kind::COMPLETE, options, Object::STR, origin) { set_mode(mode() | Mode::INHERIT); }
    GenericFile(Origin origin)                    : GenericFile(Options{}, origin) {}
    GenericFile(Options options)                  : GenericFile(options, Origin::MEMORY) {}
    GenericFile()                                 : GenericFile(Options{}, Origin::MEMORY) {}

    DataSource* new_instance(const Object& target, Origin origin) const override { return new GenericFile(origin); }

    void read(const Object& target) override;
    void write(const Object& target, const Object& cache) override;
};

inline
void GenericFile::read(const Object& target) {
    auto fpath = path(target).string();
    auto size = std::filesystem::file_size(fpath);
    std::ifstream f_in{fpath, std::ios::in | std::ios::binary};
    String str;
    str.resize(size);
    f_in.read(str.data(), size);
    read_set(target, std::move(str));

    if (f_in.bad()) report_read_error(fpath, strerror(errno));
    if (f_in.fail()) report_read_error(fpath, "ostream::fail");
}

inline
void GenericFile::write(const Object& target, const Object& cache) {
    auto fpath = path(target).string();
    auto& str = cache.as<String>();
    std::ofstream f_out{fpath, std::ios::out};
    f_out.exceptions(std::ifstream::failbit);
    f_out.write(&str[0], str.size());

    if (f_out.bad()) report_write_error(fpath, strerror(errno));
    if (f_out.fail()) report_write_error(fpath, "ostream::fail");
}

} // namespace filesystem
} // namespace nodel

