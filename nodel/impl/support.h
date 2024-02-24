#pragma once

#include <string>
#include <charconv>
#include <iomanip>
#include <concepts>

#include "types.h"

namespace nodel {

// std::visit support
template<class... Ts>
struct overloaded : Ts... { using Ts::operator()...; };
// explicit deduction guide (not needed as of C++20)
template<class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

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
    assert(err == std::errc());
    str.resize(ptr - str.data());
    return str;
}

inline
std::string float_to_str(double v) {
    // IEEE 754-1985 - max digits=24
    std::string str(24, ' ');
    auto [ptr, ec] = std::to_chars(str.data(), str.data() + str.size(), v);
    assert(ec == std::errc());
    str.resize(ptr - str.data());
    return str;
}

inline
bool str_to_bool(const std::string& str) {
    // TODO: throw errors?
    return (str == "true" || str == "1");
}

inline
Int str_to_int(const std::string& str) {
    // TODO: throw errors?
    Int value;
    const char* beg = str.data();
    const char* end = beg + str.size();
    auto [ptr, ec] = std::from_chars(beg, end, value);
    assert (ec == std::errc());
    return value;
}

inline
UInt str_to_uint(const std::string& str) {
    // TODO: throw errors?
    UInt value;
    const char* beg = str.data();
    const char* end = beg + str.size();
    auto [ptr, ec] = std::from_chars(beg, end, value);
    assert (ec == std::errc());
    return value;
}

inline
Float str_to_float(const std::string& str) {
    // TODO: throw errors?
    const char* beg = str.data();
    char* end = nullptr;
    Float value = std::strtold(beg, &end);
    return value;
}

} // nodel namespace
