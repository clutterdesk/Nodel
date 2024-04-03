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

#include <nodel/format/json.h>
#include <nodel/core/Object.h>

#include <fstream>

namespace nodel {
namespace filesystem {

class JsonFile : public File
{
  public:
    JsonFile(Origin origin) : File(Kind::COMPLETE, Mode::ALL | Mode::INHERIT, origin) {}
    JsonFile()              : File(Kind::COMPLETE, Mode::ALL | Mode::INHERIT, Origin::MEMORY) {}

    DataSource* new_instance(const Object& target, Origin origin) const override { return new JsonFile(origin); }

    void read_type(const Object& target) override;
    void read(const Object& target) override;
    void write(const Object& target, const Object& cache, bool quiet) override;
};

inline
void JsonFile::read_type(const Object& target) {
    auto fpath = path(target).string();
    std::ifstream f_in{fpath, std::ios::in};
    json::impl::Parser parser{nodel::impl::StreamAdapter{f_in}};
    read_set(target, (Object::ReprType)parser.parse_type());
}

inline
void JsonFile::read(const Object& target) {
    auto fpath = path(target).string();
    std::string error;
    read_set(target, json::parse_file(fpath, error));
    if (error.size() > 0) set_failed(true);
}

inline
void JsonFile::write(const Object& target, const Object& cache, bool quiet) {
    auto fpath = path(target).string();
    std::ofstream f_out{fpath, std::ios::out};
    cache.to_json(f_out);
    // TODO: quiet
}

} // namespace filesystem
} // namespace nodel

