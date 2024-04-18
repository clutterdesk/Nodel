#pragma once

#include <nodel/parser/json.h>
#include <nodel/core/Object.h>

#include <fstream>

namespace nodel {
namespace filesystem {

class JsonFile : public File
{
  public:
    JsonFile(Options options, Origin origin)   : File(Kind::COMPLETE, options, origin) { set_mode(mode() | Mode::INHERIT); }
    JsonFile(Origin origin)                    : JsonFile(Options{}, origin) {}
    JsonFile(Options options = {})             : JsonFile(options, Origin::MEMORY) {}

    DataSource* new_instance(const Object& target, Origin origin) const override { return new JsonFile(origin); }

    void read_type(const Object& target) override;
    void read(const Object& target) override;
    void write(const Object& target, const Object& cache) override;
};

inline
void JsonFile::read_type(const Object& target) {
    auto fpath = path(target).string();
    std::ifstream f_in{fpath, std::ios::in};
    json::impl::Parser parser{nodel::parse::StreamAdapter{f_in}};
    read_set(target, (Object::ReprIX)parser.parse_type());
}

inline
void JsonFile::read(const Object& target) {
    auto fpath = path(target).string();
    std::string error;
    read_set(target, json::parse_file(fpath, error));
    if (error.size() > 0) report_read_error(fpath, error);
}

inline
void JsonFile::write(const Object& target, const Object& cache) {
    auto fpath = path(target).string();
    std::ofstream f_out{fpath, std::ios::out};
    cache.to_json(f_out);

    if (f_out.bad()) report_write_error(fpath, strerror(errno));
    if (f_out.fail()) report_write_error(fpath, "ostream::fail()");
}

} // namespace filesystem
} // namespace nodel

