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
    // There are 53-bits in IEEE 754 (64-bit float) standard, and log10(2**53) equals 15.95, so
    // round to 15 digits precision.
    auto len = std::snprintf(buf, 25, "%.15g", v);
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
    ASSERT(result.ec == std::errc());
    return value;
}

inline
UInt str_to_uint(const StringView& str) {
    // TODO: throw errors?
    UInt value;
    const char* beg = str.data();
    const char* end = beg + str.size();
    auto result = std::from_chars(beg, end, value);
    ASSERT(result.ec == std::errc());
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

inline
StringView substr(const StringView& str, int start) {
    if (start < 0) start += str.size();
    return str.substr(start);
}

inline
StringView substr(const StringView& str, int start, int end) {
    if (start < 0) start += str.size();
    if (end < 0) end += str.size();
    return str.substr(start, end);
}

inline
StringView ltrim(const StringView& str) {
    auto it = str.cbegin();
    auto end = str.cend();
    while (std::isspace(*it) && it != end) ++it;
    return str.substr(std::distance(str.cbegin(), it));
}

inline
StringView rtrim(const StringView& str) {
    auto it = str.crbegin();
    auto end = str.crend();
    while (std::isspace(*it) && it != end) ++it;
    return str.substr(0, str.size() - std::distance(str.crbegin(), it));
}

inline
StringView trim(const StringView& str) {
    // ltrim
    auto lit = str.cbegin();
    auto lend = str.cend();
    while (std::isspace(*lit) && lit != lend) ++lit;

    // rtrim
    auto rit = str.crbegin();
    auto rend = str.crend();
    while (std::isspace(*rit) && rit != rend) ++rit;

    return str.substr(std::distance(str.cbegin(), lit), str.size() - std::distance(str.crbegin(), rit));
}

} // nodel namespace

