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

#include <nodel/types.h>

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


struct Interval
{
    Interval() = default;

    Interval(Endpoint min, Endpoint max) : m_min{min}, m_max{max} {
        if (m_min.m_kind == Endpoint::Kind::DEFAULT) m_min.m_kind = Endpoint::Kind::CLOSED;
        if (m_max.m_kind == Endpoint::Kind::DEFAULT) m_max.m_kind = Endpoint::Kind::OPEN;
    }

    bool is_empty() const { return m_min.m_value == null && m_max.m_value == null; }

    bool contains(const Key& key) const;

    std::pair<UInt, UInt> to_indices(size_t list_size) const;

    std::string to_str() const;

    const Endpoint& min() const { return m_min; }
    const Endpoint& max() const { return m_max; }

    Endpoint m_min;
    Endpoint m_max;
};


inline
bool Interval::contains(const Key& key) const {
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
std::pair<UInt, UInt> Interval::to_indices(size_t list_size) const {
    UInt min_i;
    UInt max_i;

    switch (m_min.m_value.type()) {
        case Key::NULL_I: {
            min_i = 0;
            assert (m_max.m_kind == Endpoint::Kind::CLOSED);
            break;
        }
        case Key::INT_I: {
            min_i = (m_min.m_kind == Endpoint::Kind::OPEN)?
                    (m_min.m_value.to_int() + 1):
                    m_min.m_value.to_int();
            break;
        }
        case Key::UINT_I: {
            min_i = (m_min.m_kind == Endpoint::Kind::OPEN)?
                    (m_min.m_value.to_uint() + 1):
                    m_min.m_value.to_uint();
            break;
        }
        default:
            throw WrongType(Key::type_name(m_min.m_value.type()));
    }

    switch (m_max.m_value.type()) {
        case Key::NULL_I: {
            max_i = list_size;
            assert (m_max.m_kind == Endpoint::Kind::OPEN);
            break;
        }
        case Key::INT_I: {
            max_i = (m_max.m_kind == Endpoint::Kind::OPEN)?
                    m_max.m_value.to_int():
                    (m_max.m_value.to_int() + 1);
            break;
        }

        case Key::UINT_I: {
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
std::string Interval::to_str() const {
    std::stringstream ss;
    ss << ((m_min.m_kind == Endpoint::Kind::OPEN)? '(': '[');
    ss << m_min.m_value.to_str() << ", ";
    ss << m_max.m_value.to_str();
    ss << ((m_max.m_kind == Endpoint::Kind::OPEN)? ')': ']');
    return ss.str();
}

} // nodel namespace
