/// @b License: @n Apache License v2.0
/// @copyright Robert Dunnagan
#pragma once

#include <nodel/core/Object.hxx>
#include <sstream>

namespace nodel::filesystem {

/////////////////////////////////////////////////////////////////////////////
/// Base class for all file DataSource classes
/////////////////////////////////////////////////////////////////////////////
class File : public DataSource
{
  public:
    File(Kind kind, Origin origin, Multilevel mlvl)
    : DataSource(kind, origin, mlvl) { set_mode(mode() | Mode::INHERIT); }

    File(Kind kind, ReprIX repr_ix, Origin origin, Multilevel mlvl)
    : DataSource(kind, repr_ix, origin, mlvl) { set_mode(mode() | Mode::INHERIT); }

    File(Kind kind, Origin origin)                 : File(kind, origin, Multilevel::NO) {}
    File(Kind kind, ReprIX repr_ix, Origin origin) : File(kind, repr_ix, origin, Multilevel::NO) {}

    void configure(const Object& uri) override { DataSource::configure(uri); };

  protected:
    void report_read_error(const std::string& path, const std::string& error);
    void report_write_error(const std::string& path, const std::string& error);
};


inline
void File::report_read_error(const std::string& path, const std::string& error) {
    std::stringstream ss;
    ss << error << " (" << path << ')';
    DataSource::report_read_error(ss.str());
}

inline
void File::report_write_error(const std::string& path, const std::string& error) {
    std::stringstream ss;
    ss << error << " (" << path << ')';
    DataSource::report_write_error(ss.str());
}

} // namespace nodel::filesystem
