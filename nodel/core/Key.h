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

#include <string>
#include <cstring>
#include <charconv>
#include <iomanip>

#include <nodel/support/logging.h>
#include <nodel/support/string.h>
#include <nodel/support/intern.h>
#include <nodel/support/exception.h>
#include <nodel/types.h>

using namespace std::literals::string_literals;
using namespace std::literals::string_view_literals;


namespace nodel {

struct WrongType : public NodelException
{
    static std::string make_message(const std::string_view& actual) {
        std::stringstream ss;
        ss << "type=" << actual;
        return ss.str();
    }

    static std::string make_message(const std::string_view& actual, const std::string_view& expected) {
        std::stringstream ss;
        ss << "type=" << actual << ", expected=" << expected;
        return ss.str();
    }

    WrongType(const std::string_view& actual) : NodelException(make_message(actual)) {}
    WrongType(const std::string_view& actual, const std::string_view& expected) : NodelException(make_message(actual, expected)) {}
};


class KeyIterator;

class Key
{
  public:
    enum {
        EMPTY_I,
        NULL_I,   // json null
        BOOL_I,
        INT_I,
        UINT_I,
        FLOAT_I,
        STR_I
    };

  private:
    union Repr {
        Repr()           : z{(void*)0} {}
        Repr(bool v)     : b{v} {}
        Repr(Int v)      : i{v} {}
        Repr(UInt v)     : u{v} {}
        Repr(Float v)    : f{v} {}
        Repr(intern_t s) : s{s} {}
        ~Repr() {}

        void* z;
        bool  b;
        Int   i;
        UInt  u;
        Float f;
        intern_t s;
    };

  public:
    static std::string_view type_name(uint8_t repr_ix) {
        switch (repr_ix) {
            case NULL_I:  return "null";
            case BOOL_I:  return "bool";
            case INT_I:   return "int";
            case UINT_I:  return "uint";
            case FLOAT_I: return "float";
            case STR_I:   return "string";
            default:      throw std::logic_error("invalid repr_ix");
        }
    }

  public:
    Key()                     : m_repr{}, m_repr_ix{NULL_I} {}
    Key(null_t)               : m_repr{}, m_repr_ix{NULL_I} {}
    Key(is_bool auto v)       : m_repr{v}, m_repr_ix{BOOL_I} {}
    Key(is_like_Float auto v) : m_repr{v}, m_repr_ix{FLOAT_I} {}
    Key(is_like_Int auto v)   : m_repr{(Int)v}, m_repr_ix{INT_I} {}
    Key(is_like_UInt auto v)  : m_repr{(UInt)v}, m_repr_ix{UINT_I} {}
    Key(intern_t s)           : m_repr{s}, m_repr_ix{STR_I} {}
    Key(const String& s)      : m_repr{intern_string(s)}, m_repr_ix{STR_I} {}
    Key(const StringView& s)  : m_repr{intern_string(s)}, m_repr_ix{STR_I} {}

    ~Key() { release(); }

    Key(const Key& key) {
        m_repr_ix = key.m_repr_ix;
        switch (m_repr_ix) {
            case NULL_I:  m_repr.z = key.m_repr.z; break;
            case BOOL_I:  m_repr.b = key.m_repr.b; break;
            case INT_I:   m_repr.i = key.m_repr.i; break;
            case UINT_I:  m_repr.u = key.m_repr.u; break;
            case FLOAT_I: m_repr.f = key.m_repr.f; break;
            case STR_I:   m_repr.s = key.m_repr.s; break;
        }
    }

    Key(Key&& key) {
        m_repr_ix = key.m_repr_ix;
        if (m_repr_ix == STR_I) {
            m_repr.s = key.m_repr.s;
        } else {
            // generic memory copy
            m_repr.u = key.m_repr.u;
        }
        key.m_repr.z = (void*)0;
        key.m_repr_ix = NULL_I;
    }

    Key& operator = (const Key& key) {
        release();
        m_repr_ix = key.m_repr_ix;
        switch (m_repr_ix) {
            case NULL_I:  m_repr.z = key.m_repr.z; break;
            case BOOL_I:  m_repr.b = key.m_repr.b; break;
            case INT_I:   m_repr.i = key.m_repr.i; break;
            case UINT_I:  m_repr.u = key.m_repr.u; break;
            case FLOAT_I: m_repr.f = key.m_repr.f; break;
            case STR_I:   m_repr.s = key.m_repr.s; break;
        }
        return *this;
    }

    Key& operator = (null_t)               { release(); m_repr.z = nullptr; m_repr_ix = NULL_I; return *this; }
    Key& operator = (is_bool auto v)       { release(); m_repr.b = v; m_repr_ix = BOOL_I; return *this; }
    Key& operator = (is_like_Int auto v)   { release(); m_repr.i = v; m_repr_ix = INT_I; return *this; }
    Key& operator = (is_like_UInt auto v)  { release(); m_repr.u = v; m_repr_ix = UINT_I; return *this; }
    Key& operator = (Float v)              { release(); m_repr.f = v; m_repr_ix = FLOAT_I; return *this; }

    Key& operator = (const String& s) {
        release();
        m_repr.s = intern_string(s);
        m_repr_ix = STR_I;
        return *this;
    }

    Key& operator = (intern_t s) {
        release();
        m_repr.s = s;
        m_repr_ix = STR_I;
        return *this;
    }

    bool operator == (const Key& other) const {
        switch (m_repr_ix) {
            case NULL_I:  return other == null;
            case BOOL_I:  return other == m_repr.b;
            case INT_I:   return other == m_repr.i;
            case UINT_I:  return other == m_repr.u;
            case FLOAT_I: return other == m_repr.f;
            case STR_I:   return other == m_repr.s;
            default:      break;
        }
        return false;
    }

    bool operator == (null_t) const {
        return m_repr_ix == NULL_I;
    }

    bool operator == (intern_t s) const {
        if (m_repr_ix == STR_I) return s == m_repr.s;
        else return false;
    }

    bool operator == (is_like_string auto&& other) const {
        return (m_repr_ix == STR_I)? (m_repr.s == other): false;
    }

    bool operator == (is_number auto other) const {
        switch (m_repr_ix) {
            case BOOL_I:  return m_repr.b == other;
            case INT_I:   return m_repr.i == other;
            case UINT_I:  return m_repr.u == other;
            case FLOAT_I: return m_repr.f == other;
            default:
                return false;
        }
    }

    uint8_t type() const   { return m_repr_ix; }
    auto type_name() const { return type_name(m_repr_ix); }

    bool is_bool() const    { return m_repr_ix == BOOL_I; }
    bool is_int() const     { return m_repr_ix == INT_I; }
    bool is_uint() const    { return m_repr_ix == UINT_I; }
    bool is_any_int() const { return m_repr_ix == INT_I || m_repr_ix == UINT_I; }
    bool is_float() const   { return m_repr_ix == FLOAT_I; }
    bool is_str() const     { return m_repr_ix == STR_I; }
    bool is_num() const     { return m_repr_ix && m_repr_ix < STR_I; }

    // unsafe, but will not segv
    template <typename T> T as() const requires is_byvalue<T>;
    template <> bool as<bool>() const     { return m_repr.b; }
    template <> Int as<Int>() const       { return m_repr.i; }
    template <> UInt as<UInt>() const     { return m_repr.u; }
    template <> Float as<Float>() const   { return m_repr.f; }

    template <typename T> const T& as() const requires std::is_same<T, StringView>::value {
        if (m_repr_ix != STR_I) throw wrong_type(m_repr_ix);
        return m_repr.s.data();
    }

    bool to_bool() const {
        switch (m_repr_ix) {
            case BOOL_I:  return m_repr.b;
            case INT_I:   return (bool)m_repr.i;
            case UINT_I:  return (bool)m_repr.u;
            case FLOAT_I: return (bool)m_repr.f;
            case STR_I:   return str_to_bool(m_repr.s.data());
            default: // TODO
                return std::numeric_limits<Int>::max();
        }
    }

    Int to_int() const {
        switch (m_repr_ix) {
            case BOOL_I:  return (Int)m_repr.b;
            case INT_I:   return m_repr.i;
            case UINT_I:  return (Int)m_repr.u;
            case FLOAT_I: return (Int)m_repr.f;
            case STR_I:   return str_to_int(m_repr.s.data());
            default: // TODO
                return std::numeric_limits<Int>::max();
        }
    }

    UInt to_uint() const {
        switch (m_repr_ix) {
            case BOOL_I:  return (UInt)m_repr.b;
            case INT_I:   return (UInt)m_repr.i;
            case UINT_I:  return m_repr.u;
            case FLOAT_I: return (UInt)m_repr.f;
            case STR_I:   return str_to_uint(m_repr.s.data());
            default:  // TODO
                return std::numeric_limits<UInt>::max();
        }
    }

    Float to_float() const {
        switch (m_repr_ix) {
            case BOOL_I:  return (Float)m_repr.b;
            case INT_I:   return (Float)m_repr.i;
            case UINT_I:  return (Float)m_repr.u;
            case FLOAT_I: return m_repr.f;
            case STR_I:   return str_to_float(m_repr.s.data());
            default:  // TODO
                return std::numeric_limits<UInt>::max();
        }
    }

    void to_step(std::ostream& stream, bool is_first = false) const {
        switch (m_repr_ix) {
            case BOOL_I:  stream << (m_repr.b? "[1]": "[0]"); break;
            case INT_I:   stream << '[' << nodel::int_to_str(m_repr.i) << ']'; break;
            case UINT_I:  stream << '[' << nodel::int_to_str(m_repr.u) << ']'; break;
            case FLOAT_I: stream << '[' << nodel::float_to_str(m_repr.f) << ']'; break;
            case STR_I: {
                auto pos = m_repr.s.data().find_first_of("\".");
                if (pos != StringView::npos) {
                    stream << '[' << quoted(m_repr.s.data()) << ']';
                } else {
                    if (!is_first) stream << '.';
                    stream << m_repr.s.data();
                }
                break;
            }
            default:
                throw wrong_type(m_repr_ix);
        }
    }

    String to_str() const {
        switch (m_repr_ix) {
            case NULL_I:  return "null";
            case BOOL_I:  return m_repr.b? "true": "false";
            case INT_I:   return nodel::int_to_str(m_repr.i);
            case UINT_I:  return nodel::int_to_str(m_repr.u);
            case FLOAT_I: return nodel::float_to_str(m_repr.f);
            case STR_I:   return String{m_repr.s.data()};
            default:      throw wrong_type(m_repr_ix);
        }
    }

    String to_json() const {
        switch (m_repr_ix) {
            case NULL_I:  return "null";
            case BOOL_I:  return m_repr.b? "true": "false";
            case INT_I:   return nodel::int_to_str(m_repr.i);
            case UINT_I:  return nodel::int_to_str(m_repr.u);
            case FLOAT_I: {
                // IEEE 754-1985 - max digits=24
                std::string str(24, ' ');
                auto [ptr, err] = std::to_chars(str.data(), str.data() + str.size(), m_repr.f);
                ASSERT(err == std::errc());
                str.resize(ptr - str.data());
                return str;
            }
            case STR_I: {
                std::stringstream ss;
                ss << std::quoted(m_repr.s.data());
                return ss.str();
            }
            default:
                throw wrong_type(m_repr_ix);
        }
    }

    size_t hash() const {
        static std::hash<Float> float_hash;
        switch (m_repr_ix) {
            case NULL_I:  return 0;
            case BOOL_I:  return (size_t)m_repr.b;
            case INT_I:   return (size_t)m_repr.i;
            case UINT_I:  return (size_t)m_repr.u;
            case FLOAT_I: return (size_t)float_hash(m_repr.f);
            case STR_I:   return (size_t)m_repr.s.data().data();
            default:
                throw wrong_type(m_repr_ix);
        }
    }

    static WrongType wrong_type(uint8_t actual)                   { return type_name(actual); };
    static WrongType wrong_type(uint8_t actual, uint8_t expected) { return {type_name(actual), type_name(expected)}; };

  private:
    void release() {
        m_repr.z = nullptr;
        m_repr_ix = NULL_I;
    }

  private:
    Repr m_repr;
    uint8_t m_repr_ix;

  friend std::ostream& operator<< (std::ostream& ostream, const Key& key);
  friend class Object;
  friend class KeyIterator;
  friend Key operator ""_key (const char*);
};


inline
Key operator ""_key (const char* str, size_t size) {
    return intern_string_literal(StringView{str, size});
}

inline
std::ostream& operator<< (std::ostream& ostream, const Key& key) {
    switch (key.m_repr_ix) {
        case Key::NULL_I:  return ostream << "null";
        case Key::BOOL_I:  return ostream << key.m_repr.b;
        case Key::INT_I:   return ostream << key.m_repr.i;
        case Key::UINT_I:  return ostream << key.m_repr.u;
        case Key::FLOAT_I: return ostream << key.m_repr.f;
        case Key::STR_I:   return ostream << key.m_repr.s.data();
        default:           throw std::invalid_argument("key");
    }
}

struct KeyHash
{
    size_t operator () (const Key& key) const {
        return key.hash();
    }
};


// TODO: Make type checks functional?
//----------------------------------------------------------------------------------
// Operations
//----------------------------------------------------------------------------------
//template <> bool is_null(const Key& key)  { return key.is_null(); }
//template <> bool is_bool(const Key& key)  { return key.is_bool(); }
//template <> bool is_int(const Key& key)   { return key.is_int(); }
//template <> bool is_uint(const Key& key)  { return key.is_uint(); }
//template <> bool is_float(const Key& key) { return key.is_float(); }
//template <> bool is_str(const Key& key)   { return key.is_str(); }
//template <> bool is_num(const Key& key)   { return key.is_num(); }
//
//template <> bool to_bool(const Key& key)             { return key.to_bool(); }
//template <> Int to_int(const Key& key)               { return key.to_int(); }
//template <> UInt to_uint(const Key& key)             { return key.to_uint(); }
//template <> Float to_float(const Key& key)           { return key.to_float(); }
//template <> std::string to_str(const Key& key)       { return key.to_str(); }
//template <> std::string to_json(const Key& key)      { return key.to_json(); }

} // namespace nodel

namespace std {

//----------------------------------------------------------------------------------
// std::hash
//----------------------------------------------------------------------------------
template<>
struct hash<nodel::Key>
{
    std::size_t operator () (const nodel::Key& key) const noexcept {
      return key.hash();
    }
};

} // namespace std

#ifdef FMT_FORMAT_H_

namespace fmt {
template <>
struct formatter<nodel::Key> : formatter<const char*> {
    auto format(const nodel::Key& key, format_context& ctx) {
        std::string str = key.to_str();
        return fmt::formatter<const char*>::format((const char*)str.c_str(), ctx);
    }
};
} // namespace fmt

#endif
