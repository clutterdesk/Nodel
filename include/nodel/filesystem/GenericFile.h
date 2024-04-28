#pragma once

#include <nodel/core/Object.h>

#include <fstream>

namespace nodel {
namespace filesystem {

class GenericFile : public File
{
  public:
    GenericFile(Origin origin) : File(Kind::COMPLETE, Object::STR, origin) { set_mode(mode() | Mode::INHERIT); }
    GenericFile()              : GenericFile(Origin::MEMORY) {}

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

