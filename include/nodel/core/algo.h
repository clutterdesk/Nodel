#pragma once

#include "Object.h"
#include <cctype>
#include <cstdlib>
#include <algorithm>

namespace nodel {

template<class InRange, class UnaryPredicate>
Object find_first(InRange range, UnaryPredicate pred) {
    auto it = range.begin();
    auto end = range.end();
    for (; it != end; ++it) {
        if (pred(*it))
            return *it;
    }
    return {};
}

// TODO: move to parse.h
template <typename Iter>
void consume_whitespace(Iter& it, Iter end) {
    while (it != end && std::isspace(*it)) ++it;
}

} // namespace nodel
