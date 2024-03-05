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

#include "File.h"
#include <nodel/impl/CsvParser.h>
#include <nodel/impl/Object.h>

#include <fstream>

namespace nodel {
namespace filesystem {

class CsvFile : public File
{
  public:
    CsvFile(const std::string& ext) : File(ext) {}

    DataSource* new_instance(const Object& target) const override { return new CsvFile(ext()); }

    void read_meta(const Object& target, Object& cache) override;
    void read(const Object& target, Object& cache) override;
    void write(const Object& target, const Object& cache) override;
};

inline
void CsvFile::read_meta(const Object& target, Object& cache) {
    cache = Object(Object::LIST_I);
}

inline
void CsvFile::read(const Object& target, Object& cache) {
    auto fpath = path(target).string();
    std::string error;
    cache.refer_to(csv::parse_file(fpath, error));
    if (error.size() > 0) throw csv::CsvException(error);
}

inline
void CsvFile::write(const Object& target, const Object& cache) {
    auto fpath = path(target).string();
    std::ofstream f_out{fpath + ext(), std::ios::out};
    // TODO: unfinished
}

} // namespace filesystem
} // namespace nodel

