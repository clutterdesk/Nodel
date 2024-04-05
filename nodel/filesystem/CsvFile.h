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
#include <cerrno>

namespace nodel {
namespace filesystem {

class CsvFile : public File
{
  public:
    CsvFile(Options options, Origin origin)   : File(Kind::COMPLETE, options, Object::LIST, origin) { set_mode(mode() | Mode::INHERIT); }
    CsvFile(Origin origin)                    : CsvFile(Options{}, origin) {}
    CsvFile(Options options)                  : CsvFile(options, Origin::MEMORY) {}
    CsvFile()                                 : CsvFile(Options{}, Origin::MEMORY) {}

    DataSource* new_instance(const Object& target, Origin origin) const override { return new CsvFile(origin); }

    void read(const Object& target) override;
    void write(const Object& target, const Object& cache) override;
};

inline
void CsvFile::read(const Object& target) {
    auto fpath = path(target).string();
    std::string error;
    read_set(target, csv::parse_file(fpath, error));
    if (error.size() > 0) report_read_error(fpath, error);
}

inline
void CsvFile::write(const Object& target, const Object& cache) {
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

    if (f_out.bad()) report_write_error(fpath, strerror(errno));
    if (f_out.fail()) report_write_error(fpath, "ostream::fail()");
}

} // namespace filesystem
} // namespace nodel

