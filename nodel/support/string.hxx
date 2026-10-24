/// @b License: @n Apache License v2.0
/// @copyright Robert Dunnagan
#pragma once

#include <assert.h>
#include <string>
#include <sstream>
#include <charconv>
#include <iomanip>

#include <nodel/support/types.hxx>
#include <nodel/support/exception.hxx>

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
    // Shortest representation that parses back to the same value.
    char buf[32];
    auto [end, ec] = std::to_chars(buf, buf + sizeof(buf), v);
    assert (ec == std::errc{});
    return {buf, (size_t)(end - buf)};
}

inline
bool str_to_bool(const StringView& str) {
    return (str == "true" || str == "1");
}

struct InvalidNumber : public NodelException
{
    InvalidNumber(const StringView& str, const char* type)
      : NodelException(make_message(str, type)) {}

    static std::string make_message(const StringView& str, const char* type) {
        std::stringstream ss;
        ss << "Invalid " << type << ": " << std::quoted(std::string{str});
        return ss.str();
    }
};

namespace detail {

// Kept out of line so the throw does not bloat the inlined parse fast path.
[[noreturn]] NODEL_COLD NODEL_NOINLINE
inline void throw_invalid_number(const StringView& str, const char* type) {
    throw InvalidNumber(str, type);
}

// Parse the entire string as an integer, accepting an optional leading '+'.
// Throws InvalidNumber if the string is empty, contains anything other than
// the number, or is out of range for T.
template <typename T>
inline T str_to_integral(const StringView& str, const char* type) {
    const char* beg = str.data();
    const char* end = beg + str.size();
    if (beg != end && *beg == '+') {
        ++beg;
        if (beg != end && *beg == '-') throw_invalid_number(str, type);
    }
    T value;
    auto [ptr, ec] = std::from_chars(beg, end, value);
    if (ec != std::errc{} || ptr != end) [[unlikely]]
        throw_invalid_number(str, type);
    return value;
}

} // namespace detail

inline
Int str_to_int(const StringView& str) {
    return detail::str_to_integral<Int>(str, "integer");
}

inline
UInt str_to_uint(const StringView& str) {
    return detail::str_to_integral<UInt>(str, "unsigned integer");
}

inline
Float str_to_float(const StringView& str) {
    // TODO: throw errors?
    // strtold requires a null-terminated string, which a StringView may not be
    // (interned strings and substrings are not), so parse a terminated copy.
    char buf[64];
    if (str.size() < sizeof(buf)) {
        str.copy(buf, str.size());
        buf[str.size()] = 0;
        return std::strtold(buf, nullptr);
    }
    return std::strtold(String{str}.c_str(), nullptr);
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
    while (it != end && std::isspace(static_cast<unsigned char>(*it))) ++it;
    return str.substr(std::distance(str.cbegin(), it));
}

inline
StringView rtrim(const StringView& str) {
    auto it = str.crbegin();
    auto end = str.crend();
    while (it != end && std::isspace(static_cast<unsigned char>(*it))) ++it;
    return str.substr(0, str.size() - std::distance(str.crbegin(), it));
}

inline
StringView trim(const StringView& str) {
    return rtrim(ltrim(str));
}

} // nodel namespace

