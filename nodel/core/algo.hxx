/// @b License: @n Apache License v2.0
/// @copyright Robert Dunnagan
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
KeyList collect(KeyRange&& range) {
    KeyList keys;
    auto it = range.begin();
    auto end = range.end();
    for (; it != end; ++it)
        keys.push_back(*it);
    return keys;
}

inline
ObjectList collect(ValueRange&& range) {
    ObjectList objs;
    auto it = range.begin();
    auto end = range.end();
    for (; it != end; ++it)
        objs.push_back(*it);
    return objs;
}

inline
ItemList collect(ItemRange&& range) {
    ItemList items;
    auto it = range.begin();
    auto end = range.end();
    for (; it != end; ++it)
        items.push_back(*it);
    return items;
}

} // namespace nodel::algo
