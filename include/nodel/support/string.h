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

#include <string>
#include <sstream>
#include <charconv>
#include <iomanip>

#include <nodel/support/types.h>
#include <nodel/support/exception.h>

namespace nodel {

// string quoting, in case std::quoted method is slow
inline
std::string quoted(const std::string& str) {
  std::stringstream ss;
  ss << std::quoted(str);
  return ss.str();
}

inline
std::string int_to_str(int32_t v) {
    char buf[12];
    auto len = std::snprintf(buf, 11, "%d", v);
    assert (len > 0);
    return {buf, (size_t)len};
}

inline
std::string int_to_str(int64_t v) {
    char buf[24];
    auto len = std::snprintf(buf, 23, "%lld", v);
    assert (len > 0);
    return {buf, (size_t)len};
}

inline
std::string int_to_str(uint32_t v) {
    char buf[12];
    auto len = std::snprintf(buf, 11, "%u", v);
    assert (len > 0);
    return {buf, (size_t)len};
}

inline
std::string int_to_str(uint64_t v) {
    char buf[24];
    auto len = std::snprintf(buf, 23, "%llu", v);
    assert (len > 0);
    return {buf, (size_t)len};
}

inline
std::string float_to_str(double v) {
    char buf[26];
    auto len = std::snprintf(buf, 25, "%.17g", v);
    assert (len > 0);
    return {buf, (size_t)len};
}

inline
bool str_to_bool(const StringView& str) {
    return (str == "true" || str == "1");
}

inline
Int str_to_int(const StringView& str) {
    // TODO: throw errors?
    Int value;
    const char* beg = str.data();
    const char* end = beg + str.size();
    auto result = std::from_chars(beg, end, value);
    assert (result.ec == std::errc());
    return value;
}

inline
UInt str_to_uint(const StringView& str) {
    // TODO: throw errors?
    UInt value;
    const char* beg = str.data();
    const char* end = beg + str.size();
    auto result = std::from_chars(beg, end, value);
    assert (result.ec == std::errc());
    return value;
}

inline
Float str_to_float(const StringView& str) {
    // TODO: throw errors?
    const char* beg = str.data();
    char* end = nullptr;
    Float value = std::strtold(beg, &end);
    return value;
}

} // nodel namespace

