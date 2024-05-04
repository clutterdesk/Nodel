#pragma once

#include <nodel/parser/csv.h>
#include <nodel/core/Object.h>

#include <fstream>
#include <iomanip>
#include <cerrno>

namespace nodel {
namespace filesystem {

class CsvFile : public File
{
  public:
    CsvFile(Origin origin) : File(Kind::COMPLETE, Object::LIST, origin) { set_mode(mode() | Mode::INHERIT); }
    CsvFile()              : CsvFile(Origin::MEMORY) {}

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

    for (const auto& row : cache.iter_values()) {
        char sep = 0;
        for (const auto& col: row.iter_values()) {
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

