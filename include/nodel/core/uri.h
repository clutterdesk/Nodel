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
#include <nodel/support/types.h>
#include <nodel/support/exception.h>
#include <regex>
#include <sstream>
#include <unordered_map>
#include <mutex>

namespace nodel {

class URI;

using DataSourceFactory = std::function<DataSource*(const URI& uri)>;
using URIMap = std::unordered_map<String, DataSourceFactory>;

#define NODEL_INIT_URI_SCHEMES namespace nodel { \
    std::mutex global_uri_map_mutex; \
    URIMap global_uri_map; \
    thread_local URIMap local_uri_map; \
}

extern std::mutex global_uri_map_mutex;
extern URIMap global_uri_map;
extern thread_local URIMap local_uri_map;

struct BindError : public NodelException
{
    BindError(std::string&& err) : NodelException{std::forward<std::string>(err)} {}
};

class URI : public Object
{
  public:
    URI(const String& s) : Object{nil} { parse(s); }

  private:
    void parse(const String&);
    Object parse_uri_query(const String&);
};

inline
Object URI::parse_uri_query(const String& query) {
    Object result = Object::OMAP;
    auto it = query.cbegin();
    auto end = query.cend();
    std::string key;
    std::string val;
    std::string* p_target = &key;
    for (; it != end; ++it) {
        char c = *it;
        if (c == '=') {
            p_target = &val;
        } else if (c == '&' || c == ';') {
            p_target = &key;
            result.set(key, val);
            key.clear();
            val.clear();
        } else {
            p_target->push_back(c);
        }
    }
    if (key.size() != 0)
        result.set(key, val);
    return result;
}

inline
void URI::parse(const String& spec) {
    static std::regex uri_re{R"(([^:]+)://(([^@]+\@)?([^:/?#]*)(:(\d+))?)?((/[^?#]*)?(\?[^#]+)?(\#.*)?)?)"};

    std::smatch match;
    if (std::regex_match(spec, match, uri_re)) {
        (*(Object*)this) = Object{Object::OMAP};
        if (match[1].length() > 0) set("scheme"_key,   match[1].str());
        if (match[3].length() > 0) set("user"_key,     substr(match[3].str(), 0, -1));
        if (match[4].length() > 0) set("host"_key,     match[4].str());
        if (match[6].length() > 0) set("port"_key,     Object(match[6].str()).to_int());
        if (match[8].length() > 0) set("path"_key,     match[8].str());
        if (match[9].length() > 0) set("query"_key,    parse_uri_query(match[9].str().substr(1)));
        if (match[10].length() > 0) set("fragment"_key, match[10].str().substr(1));
    }
}

template <typename Func>
void register_uri_scheme(const String& scheme, Func&& func) {
    local_uri_map[scheme] = std::forward<Func>(func);
    std::scoped_lock lock{global_uri_map_mutex};
    global_uri_map[scheme] = std::forward<Func>(func);
}

inline
void remove_uri_scheme(const String& scheme) {
    local_uri_map.erase(scheme);
    std::scoped_lock lock{global_uri_map_mutex};
    global_uri_map.erase(scheme);
}

inline
DataSourceFactory lookup_uri_scheme(const String& scheme) {
    if (local_uri_map.size() == 0) {
        std::scoped_lock lock{global_uri_map_mutex};
        local_uri_map = global_uri_map;
        if (auto it = local_uri_map.find(scheme); it != local_uri_map.end())
            return std::get<1>(*it);
    } else {
        // If scheme is not found in local_uri_map, it might have been registered after the first
        // lookup for this thread.  In that case, we check the global_uri_map, with locking, and
        // then update the local_uri_map.
        auto it = local_uri_map.find(scheme);
        if (it != local_uri_map.end()) {
            return std::get<1>(*it);
        } else {
            std::scoped_lock lock{global_uri_map_mutex};
            it = global_uri_map.find(scheme);
            if (it != global_uri_map.end()) {
                local_uri_map = global_uri_map;
                return std::get<1>(*it);
            }
        }
    }
    return {};
}

inline
Object bind(const URI& uri, const DataSource::Options& options, Object obj) {
    auto scheme = uri.get("scheme"_key).as<String>();
    auto factory = lookup_uri_scheme(scheme);
    if (factory) {
        auto p_ds = factory(uri);
        if (obj == nil) obj = p_ds; else p_ds->bind(obj);
        p_ds->configure(uri);
        return obj;
    } else {
        std::stringstream ss;
        ss << "URI scheme not found: " << scheme;
        throw BindError(ss.str());
    }
}

inline Object bind(const URI& uri)                                     { return bind(uri, {}, nil); }
inline Object bind(const URI& uri, Object obj)                         { return bind(uri, {}, obj); }
inline Object bind(const URI& uri, const DataSource::Options& options) { return bind(uri, options, nil); }

inline
URI operator ""_uri(const char* str, size_t size) {
    return URI{String{str, size}};
}

} // namespace nodel
