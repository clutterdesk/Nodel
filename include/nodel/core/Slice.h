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

#include <nodel/support/types.h>
#include <nodel/support/string.h>
#include <nodel/support/exception.h>
#include <regex>

namespace nodel {

struct Endpoint
{
    enum class Kind { OPEN, CLOSED, DEFAULT};

    Endpoint() = default;
    Endpoint(const Key& value)            : m_value{value} {}
    Endpoint(const Key& value, Kind kind) : m_value{value}, m_kind{kind} {}

    bool is_open() const { return m_kind == Kind::OPEN; }
    const Key& value() const { return m_value; }

    Key m_value;
    Kind m_kind = Kind::DEFAULT;
};


struct Slice
{
    Slice() = default;

    Slice(Endpoint min, Endpoint max, Int step=1) : m_min{min}, m_max{max}, m_step{step} {
        if (m_min.m_kind == Endpoint::Kind::DEFAULT) m_min.m_kind = Endpoint::Kind::CLOSED;
        if (m_max.m_kind == Endpoint::Kind::DEFAULT) m_max.m_kind = Endpoint::Kind::OPEN;
    }

    bool is_empty() const { return m_min.m_kind == Endpoint::Kind::DEFAULT; }

    bool contains(const Key& key) const;

    std::tuple<Int, Int, Int> to_indices(UInt size) const;
    Slice normalize(UInt size) const;

    std::string to_str() const;

    const Endpoint& min() const { return m_min; }
    const Endpoint& max() const { return m_max; }

    Endpoint m_min;
    Endpoint m_max;
    Int m_step;
};


inline
bool Slice::contains(const Key& key) const {
    // TODO: ideally, the slice would be normalized before being passed to the iterators
    //       allowing step != 1 to be handled, here
    if (m_min.m_value != nil) {
        if (m_min.m_kind == Endpoint::Kind::OPEN) {
            if (key <= m_min.m_value)
                return false;
        } else {
            if (key < m_min.m_value)
                return false;
        }
    }

    if (m_max.m_value != nil) {
        if (m_max.m_kind == Endpoint::Kind::OPEN) {
            if (key >= m_max.m_value)
                return false;
        } else {
            if (key > m_max.m_value)
                return false;
        }
    }

    return true;
}

inline
std::tuple<Int, Int, Int> Slice::to_indices(UInt size) const {
    Int min_i;
    Int max_i;

    switch (m_min.m_value.type()) {
        case Key::NIL: {
            min_i = (m_step > 0)? 0: size - 1;
            ASSERT(m_min.m_kind == Endpoint::Kind::CLOSED);
            break;
        }
        case Key::INT: {
            min_i = m_min.m_value.to_int();
            if (min_i < 0) min_i += size;
            if (m_min.m_kind == Endpoint::Kind::OPEN) ++min_i;
            break;
        }
        case Key::UINT: {
            min_i = (m_min.m_kind == Endpoint::Kind::OPEN)?
                    (m_min.m_value.to_uint() + 1):
                    m_min.m_value.to_uint();
            break;
        }
        default:
            throw WrongType(Key::type_name(m_min.m_value.type()));
    }

    switch (m_max.m_value.type()) {
        case Key::NIL: {
            max_i = (m_step > 0)? size: -1;
            ASSERT(m_max.m_kind == Endpoint::Kind::OPEN);
            break;
        }
        case Key::INT: {
            max_i = m_max.m_value.to_int();
            if (max_i < 0) max_i += size;
            if (m_max.m_kind == Endpoint::Kind::CLOSED) ++max_i;
            break;
        }
        case Key::UINT: {
            max_i = (m_max.m_kind == Endpoint::Kind::OPEN)?
                    m_max.m_value.to_uint():
                    (m_max.m_value.to_uint() + 1);
            break;
        }
        default:
            throw WrongType(Key::type_name(m_min.m_value.type()));
    }

    return std::make_tuple(min_i, max_i, m_step);
}

inline
Slice Slice::normalize(UInt size) const {
    auto [start, stop, step] = to_indices(size);
    return {{start, Endpoint::Kind::CLOSED}, {stop, Endpoint::Kind::OPEN}, step};
}

inline
std::string Slice::to_str() const {
    std::stringstream ss;
    ss << ((m_min.m_kind == Endpoint::Kind::OPEN)? '(': '[');
    ss << m_min.m_value.to_str() << ", ";
    ss << m_max.m_value.to_str();
    ss << ((m_max.m_kind == Endpoint::Kind::OPEN)? ')': ']');
    return ss.str();
}

template <typename T>
T get_slice(const T& array, Int start, Int stop, Int step) {
    T result;
    auto it = array.cbegin() + start;
    auto end = array.cbegin() + stop;
    if (step > 0) {
        for (; it < end; it += step)
            result.push_back(*it);
    } else {
        for (; it > end; it += step)
            result.push_back(*it);
    }
    return result;
}

template <class T>
void set_slice(T& l_arr, Int start, Int stop, Int step, const T& r_arr) {
    auto [count, tail] = std::div(stop - start, step);

    auto l_it = l_arr.begin() + start;
    auto l_end = l_arr.end() - tail;
    auto r_it = r_arr.begin();
    auto r_end = r_arr.end();

    for (; l_it < l_end && r_it < r_end; l_it += step, ++r_it)
        *l_it = *r_it;

    typename T::size_type l_pos = std::distance(l_it, l_end);
    for (; l_pos < l_arr.size(); l_pos += (step - 1))
        l_arr.erase(l_arr.begin() + l_pos);
}

inline
Slice operator ""_slice (const char* str, size_t size) {
    static std::regex slice_re{"([-+]?[0-9]+)?(\\:([-+]?[0-9]+)?)?(\\:([-+]?[0-9]+)?)?"};

    std::cmatch match;
    if (!std::regex_match(str, match, slice_re))
        return {};

    auto s_start = match[1];
    auto s_stop  = match[3];
    auto s_step  = match[5];

    auto step = (s_step.length() == 0)? (Int)1: str_to_int(s_step.str());
    if (step == 0) return {};

    if (s_start.length() == 0) {
        if (s_stop.length() == 0) {
            if (match[2].length() == 0) {
                return {};  // invalid
            } else {
                return {{nil, Endpoint::Kind::CLOSED}, {nil, Endpoint::Kind::OPEN}, step};
            }
        } else {
            auto stop = str_to_int(s_stop.str());
            return {{nil, Endpoint::Kind::CLOSED}, {stop, Endpoint::Kind::OPEN}, step};
        }
    } else {
        auto start = str_to_int(s_start.str());
        if (s_stop.length() == 0) {
            if (match[2].length() == 0) {
                return {{start, Endpoint::Kind::CLOSED}, {start, Endpoint::Kind::CLOSED}, step};
            } else {
                return {{start, Endpoint::Kind::CLOSED}, {nil, Endpoint::Kind::OPEN}, step};
            }
        } else {
            auto stop = str_to_int(s_stop.str());
            return {{start, Endpoint::Kind::CLOSED}, {stop, Endpoint::Kind::OPEN}, step};
        }
    }
}

} // nodel namespace
