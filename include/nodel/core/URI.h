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
#include <regex>

namespace nodel {

namespace impl {

struct URI
{
    static std::regex uri_re{R"(([^:]+):(//([^@]+\@)?([^:]+):(\d+))?((/[^?#]+)(\?[^#]+)?(\#.*)?)?)"};
    URI(const std::string_view& spec);
    Object parts;
};

inline
URI::URI(const std::string_view& spec) : m_parts{Object::OMAP} {
    std::smatch match;
    if (std::regex_match(spec, match, uri_re)) {
        if (match[1].length() > 0) parts.set("schema"_key,   match[1].str());
        if (match[3].length() > 0) parts.set("user"_key,     substr(match[3].str(), 0, -1));
        if (match[4].length() > 0) parts.set("host"_key,     match[4].str());
        if (match[5].length() > 0) parts.set("port"_key,     Object(match[5].str()).to_int());
        if (match[7].length() > 0) parts.set("path"_key,     match[7].str());
        if (match[8].length() > 0) parts.set("query"_key,    match[8].str().substr(1));
        if (match[9].length() > 0) parts.set("fragment"_key, match[9].str().substr(1));
    }
}

} // namespace impl

inline
Object parse_uri(const std::string_view& spec) {
    URI uri{spec};
    return uri.parts;
}

} // namespace nodel
