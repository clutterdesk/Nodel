/// @b License: @n Apache License v2.0
/// @copyright Robert Dunnagan
#pragma once

#include <limits>

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

} // namespace nodel
