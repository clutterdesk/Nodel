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

namespace nodel {

struct Endpoint
{
    enum class Kind { OPEN, CLOSED, DEFAULT};

    Endpoint() = default;
    Endpoint(const Key& value)            : m_value{value}, m_kind{Kind::DEFAULT} {}
    Endpoint(const Key& value, Kind kind) : m_value{value}, m_kind{kind} {}

    bool is_open() const { return m_kind == Kind::OPEN; }
    const Key& value() const { return m_value; }

    Key m_value;
    Kind m_kind = Kind::DEFAULT;
};


struct Slice
{
    Slice() = default;

    Slice(Endpoint min, Endpoint max) : m_min{min}, m_max{max} {
        if (m_min.m_kind == Endpoint::Kind::DEFAULT) m_min.m_kind = Endpoint::Kind::CLOSED;
        if (m_max.m_kind == Endpoint::Kind::DEFAULT) m_max.m_kind = Endpoint::Kind::OPEN;
    }

    bool is_empty() const { return m_min.m_value == nil && m_max.m_value == nil; }

    bool contains(const Key& key) const;

    std::pair<Int, Int> to_indices(UInt size) const;
    Slice normalize(UInt size) const;

    std::string to_str() const;

    const Endpoint& min() const { return m_min; }
    const Endpoint& max() const { return m_max; }

    Endpoint m_min;
    Endpoint m_max;
};


inline
bool Slice::contains(const Key& key) const {
    if (m_min.m_kind == Endpoint::Kind::OPEN) {
        if (key <= m_min.m_value)
            return false;
    } else {
        if (key < m_min.m_value)
            return false;
    }

    if (m_max.m_kind == Endpoint::Kind::OPEN) {
        if (key >= m_max.m_value)
            return false;
    } else {
        if (key > m_max.m_value)
            return false;
    }

    return true;
}

inline
std::pair<Int, Int> Slice::to_indices(UInt size) const {
    Int min_i;
    Int max_i;

    switch (m_min.m_value.type()) {
        case Key::NIL: {
            min_i = 0;
            assert (m_max.m_kind == Endpoint::Kind::CLOSED);
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
            max_i = size;
            assert (m_max.m_kind == Endpoint::Kind::OPEN);
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

    return std::make_pair(min_i, max_i);
}

inline
Slice Slice::normalize(UInt size) const {
    auto indices = to_indices(size);
    return {{indices.first, Endpoint::Kind::CLOSED}, {indices.second, Endpoint::Kind::OPEN}};
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

inline
Slice operator ""_slice (const char* str, size_t size) {
    // TODO: support <num>:<num>:<num> slices
    // <num>
    // <num>:<num>
    StringView sv{str, size};
    sv = ltrim(rtrim(sv));
    auto sep_pos = sv.find(':');
    if (sep_pos == StringView::npos) {
        auto start = str_to_int(sv);
        return {{start, Endpoint::Kind::CLOSED}, {start, Endpoint::Kind::CLOSED}};
    } else if (sep_pos == 0) {
        if (sep_pos == sv.size() - 1) {
            return {{0, Endpoint::Kind::CLOSED}, {-1, Endpoint::Kind::CLOSED}};
        } else {
            auto end = str_to_int(sv.substr(sep_pos + 1));
            return {{0, Endpoint::Kind::CLOSED}, {end, Endpoint::Kind::OPEN}};
        }
    } else if (sep_pos == sv.size() - 1) {
        auto start = str_to_int(sv.substr(0, sep_pos));
        return {{start, Endpoint::Kind::CLOSED}, {-1, Endpoint::Kind::CLOSED}};
    } else {
        auto start = str_to_int(sv.substr(0, sep_pos));
        auto end = str_to_int(sv.substr(sep_pos + 1));
        return {{start, Endpoint::Kind::CLOSED}, {end, Endpoint::Kind::OPEN}};
    }
}


} // nodel namespace
