#pragma once

#include <nodel/core/Object.h>
#include <sstream>

namespace nodel::filesystem {

/////////////////////////////////////////////////////////////////////////////
/// @brief Base class for all file DataSource classes
/////////////////////////////////////////////////////////////////////////////
class File : public DataSource
{
  public:
    File(Kind kind, Origin origin) : DataSource(kind, origin) {
        set_mode(mode() | Mode::INHERIT);
    }

    File(Kind kind, ReprIX repr_ix, Origin origin) : DataSource(kind, repr_ix, origin) {
        set_mode(mode() | Mode::INHERIT);
    }

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
