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
#include <charconv>
#include <iomanip>

#include <nodel/types.h>
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
std::string int_to_str(auto v) {
    // max digits=20
    std::string str(20, ' ');
    auto [ptr, err] = std::to_chars(str.data(), str.data() + str.size(), v);
    ASSERT(err == std::errc());
    str.resize(ptr - str.data());
    return str;
}

inline
std::string float_to_str(double v) {
    // IEEE 754-1985 - max digits=24
    std::string str(24, ' ');
    auto [ptr, ec] = std::to_chars(str.data(), str.data() + str.size(), v);
    ASSERT(ec == std::errc());
    str.resize(ptr - str.data());
    return str;
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
    auto [ptr, ec] = std::from_chars(beg, end, value);
    assert (ec == std::errc());
    return value;
}

inline
UInt str_to_uint(const StringView& str) {
    // TODO: throw errors?
    UInt value;
    const char* beg = str.data();
    const char* end = beg + str.size();
    auto [ptr, ec] = std::from_chars(beg, end, value);
    assert (ec == std::errc());
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

