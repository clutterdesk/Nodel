/// @b License: @n Apache License v2.0
/// @copyright Robert Dunnagan
#pragma once

#include <cmath>
#include <compare>
#include <limits>
#include <nodel/support/types.hxx>

namespace nodel {

inline
bool equal(UInt a, Int b) {
    return a <= std::numeric_limits<Int>::max() && (Int)a == b;
}

inline
std::partial_ordering compare(UInt a, Int b) {
    if (a > std::numeric_limits<Int>::max()) {
        return std::partial_ordering::greater;
    } else {
        Int ia = (Int)a;
        if (ia > b) {
            return std::partial_ordering::greater;
        } else if (ia == b) {
            return std::partial_ordering::equivalent;
        } else {
            return std::partial_ordering::less;
        }
    }
}

inline
std::partial_ordering compare(Int a, UInt b) {
    if (b > std::numeric_limits<Int>::max()) {
        return std::partial_ordering::less;
    } else {
        Int ib = (Int)b;
        if (ib > a) {
            return std::partial_ordering::less;
        } else if (ib == a) {
            return std::partial_ordering::equivalent;
        } else {
            return std::partial_ordering::greater;
        }
    }
}

/// Exact comparison of an integer and a float, without converting the
/// integer to Float (which rounds integers above 2^53).
inline
std::partial_ordering compare(Int a, Float b) {
    constexpr Float two_63 = 9223372036854775808.0;  // 2^63
    if (std::isnan(b)) return std::partial_ordering::unordered;
    if (b >= two_63)   return std::partial_ordering::less;
    if (b < -two_63)   return std::partial_ordering::greater;
    Float tb = std::trunc(b);
    Int ib = (Int)tb;
    if (a != ib) return a <=> ib;
    return 0.0 <=> (b - tb);
}

inline
std::partial_ordering compare(UInt a, Float b) {
    constexpr Float two_64 = 18446744073709551616.0;  // 2^64
    if (std::isnan(b)) return std::partial_ordering::unordered;
    if (b >= two_64)   return std::partial_ordering::less;
    if (b < 0)         return std::partial_ordering::greater;
    Float tb = std::trunc(b);
    UInt ub = (UInt)tb;
    if (a != ub) return a <=> ub;
    return 0.0 <=> (b - tb);
}

inline std::partial_ordering compare(Float a, Int b)  { return 0 <=> compare(b, a); }
inline std::partial_ordering compare(Float a, UInt b) { return 0 <=> compare(b, a); }

inline bool equal(Int a, Float b)  { return compare(a, b) == 0; }
inline bool equal(UInt a, Float b) { return compare(a, b) == 0; }

} // namespace nodel
