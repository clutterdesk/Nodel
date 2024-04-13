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
#include <sstream>

namespace nodel::filesystem {

/**
 * @brief Base class for all file DataSource classes
 */
class File : public DataSource
{
  public:
    File(Kind kind, Options options, Origin origin) : DataSource(kind, options, origin) {
        set_mode(mode() | Mode::INHERIT);
    }

    File(Kind kind, Options options, ReprIX repr_ix, Origin origin) : DataSource(kind, options, repr_ix, origin) {
        set_mode(mode() | Mode::INHERIT);
    }

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
