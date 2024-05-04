#pragma once

#include "Object.h"
#include <cctype>
#include <cstdlib>
#include <algorithm>

namespace nodel::algo {

template<class InRange, class UnaryPredicate>
Object find_first(InRange&& range, UnaryPredicate pred) {
    auto it = range.begin();
    auto end = range.end();
    for (; it != end; ++it) {
        if (pred(*it))
            return *it;
    }
    return {};
}

template<class InRange>
size_t count(InRange&& range) {
    auto it = range.begin();
    auto end = range.end();
    size_t count = 0;
    for (; it != end; ++it, ++count);
    return count;
}

inline
Object collect(ItemRange&& range) {
    OrderedMap map;
    auto it = range.begin();
    auto end = range.end();
    for (; it != end; ++it)
        map.insert(*it);
    return map;
}

inline
Object collect(ValueRange&& range) {
    ObjectList list;
    auto it = range.begin();
    auto end = range.end();
    for (; it != end; ++it)
        list.push_back(*it);
    return list;
}

} // namespace nodel::algo
