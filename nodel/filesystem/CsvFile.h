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

#include <nodel/format/csv.h>
#include <nodel/core/Object.h>

#include <fstream>
#include <iomanip>

namespace nodel {
namespace filesystem {

class CsvFile : public File
{
  public:
    CsvFile(Origin origin) : File(Kind::COMPLETE, Mode::INHERIT, Object::LIST, origin) {}
    CsvFile()              : CsvFile(Origin::MEMORY) {}

    DataSource* new_instance(const Object& target, Origin origin) const override { return new CsvFile(origin); }

    void read(const Object& target) override;
    void write(const Object& target, const Object& cache, bool quiet) override;
};

inline
void CsvFile::read(const Object& target) {
    auto fpath = path(target).string();
    std::string error;
    read_set(target, csv::parse_file(fpath, error));
    if (error.size() > 0) set_failed(true);
}

inline
void CsvFile::write(const Object& target, const Object& cache, bool quiet) {
    auto fpath = path(target).string();
    std::ofstream f_out{fpath, std::ios::out};

    for (const auto& row : cache.values()) {
        char sep = 0;
        for (const auto& col: row.values()) {
            if (sep == 0) sep = ','; else f_out << sep;
            f_out << col.to_json();
        }
        f_out << "\n";
    }

    if (f_out.fail() || f_out.bad()) set_failed(true);
}

} // namespace filesystem
} // namespace nodel

