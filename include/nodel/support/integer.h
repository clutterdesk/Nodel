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
