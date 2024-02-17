#pragma once

#include <string>
#include <charconv>

namespace nodel {

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
    auto [ptr, err] = std::to_chars(str.data(), str.data() + str.size(), v);
    assert(err == std::errc());
    str.resize(ptr - str.data());
    return str;
}

} // namespace nodel
