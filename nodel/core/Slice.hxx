/// @b License: @n Apache License v2.0
/// @copyright Robert Dunnagan
#pragma once

#include <nodel/core/Key.hxx>
#include <nodel/support/types.hxx>
#include <nodel/support/string.hxx>
#include <nodel/support/exception.hxx>

#include <algorithm>
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
            if (size > 0 && min_i < 0) min_i = std::max((Int)0, min_i + static_cast<Int>(size));
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
            if (size > 0 && max_i < 0) max_i = std::max((Int)0, max_i + static_cast<Int>(size));
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

inline
Slice operator ""_slice (const char* str, size_t) {
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
