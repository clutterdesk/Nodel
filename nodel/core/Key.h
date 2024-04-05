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
#include <ctime>

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
    enum ReprIX {
        NIL,   // json null
        BOOL,
        INT,
        UINT,
        FLOAT,
        STR
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
    static std::string_view type_name(ReprIX repr_ix) {
        switch (repr_ix) {
            case NIL:  return "nil";
            case BOOL:  return "bool";
            case INT:   return "int";
            case UINT:  return "uint";
            case FLOAT: return "float";
            case STR:   return "string";
            default:      throw std::logic_error("invalid repr_ix");
        }
    }

  public:
    Key()                     : m_repr{}, m_repr_ix{NIL} {}
    Key(nil_t)                : m_repr{}, m_repr_ix{NIL} {}
    Key(is_bool auto v)       : m_repr{v}, m_repr_ix{BOOL} {}
    Key(is_like_Float auto v) : m_repr{v}, m_repr_ix{FLOAT} {}
    Key(is_like_Int auto v)   : m_repr{(Int)v}, m_repr_ix{INT} {}
    Key(is_like_UInt auto v)  : m_repr{(UInt)v}, m_repr_ix{UINT} {}
    Key(intern_t s)           : m_repr{s}, m_repr_ix{STR} {}
    Key(const String& s)      : m_repr{intern_string(s)}, m_repr_ix{STR} {}
    Key(const StringView& s)  : m_repr{intern_string(s)}, m_repr_ix{STR} {}

    ~Key() { release(); }

    Key(const Key& key) {
        m_repr_ix = key.m_repr_ix;
        switch (m_repr_ix) {
            case NIL:  m_repr.z = key.m_repr.z; break;
            case BOOL:  m_repr.b = key.m_repr.b; break;
            case INT:   m_repr.i = key.m_repr.i; break;
            case UINT:  m_repr.u = key.m_repr.u; break;
            case FLOAT: m_repr.f = key.m_repr.f; break;
            case STR:   m_repr.s = key.m_repr.s; break;
        }
    }

    Key& operator = (const Key& key) {
        release();
        m_repr_ix = key.m_repr_ix;
        switch (m_repr_ix) {
            case NIL:  m_repr.z = key.m_repr.z; break;
            case BOOL:  m_repr.b = key.m_repr.b; break;
            case INT:   m_repr.i = key.m_repr.i; break;
            case UINT:  m_repr.u = key.m_repr.u; break;
            case FLOAT: m_repr.f = key.m_repr.f; break;
            case STR:   m_repr.s = key.m_repr.s; break;
        }
        return *this;
    }

    Key& operator = (nil_t)               { release(); m_repr.z = nullptr; m_repr_ix = NIL; return *this; }
    Key& operator = (is_bool auto v)       { release(); m_repr.b = v; m_repr_ix = BOOL; return *this; }
    Key& operator = (is_like_Int auto v)   { release(); m_repr.i = v; m_repr_ix = INT; return *this; }
    Key& operator = (is_like_UInt auto v)  { release(); m_repr.u = v; m_repr_ix = UINT; return *this; }
    Key& operator = (Float v)              { release(); m_repr.f = v; m_repr_ix = FLOAT; return *this; }

    Key& operator = (const String& s) {
        release();
        m_repr.s = intern_string(s);
        m_repr_ix = STR;
        return *this;
    }

    Key& operator = (intern_t s) {
        release();
        m_repr.s = s;
        m_repr_ix = STR;
        return *this;
    }

    bool operator == (const Key& other) const {
        switch (m_repr_ix) {
            case NIL:  return other == nil;
            case BOOL:  return other == m_repr.b;
            case INT:   return other == m_repr.i;
            case UINT:  return other == m_repr.u;
            case FLOAT: return other == m_repr.f;
            case STR:   return other == m_repr.s;
            default:      break;
        }
        return false;
    }

    bool operator == (nil_t) const {
        return m_repr_ix == NIL;
    }

    bool operator == (intern_t s) const {
        if (m_repr_ix == STR) return s == m_repr.s;
        else return false;
    }

    bool operator == (is_like_string auto&& other) const {
        return (m_repr_ix == STR)? (m_repr.s == other): false;
    }

    bool operator == (is_number auto other) const {
        switch (m_repr_ix) {
            case BOOL:  return m_repr.b == other;
            case INT:   return m_repr.i == other;
            case UINT:  return m_repr.u == other;
            case FLOAT: return m_repr.f == other;
            default:      return false;
        }
    }

    std::partial_ordering operator <=> (const Key& other) const {
        switch (m_repr_ix) {
            case BOOL: {
                switch (other.m_repr_ix) {
                    case BOOL:  return m_repr.b <=> other.m_repr.b;
                    case INT:   return m_repr.b <=> other.to_bool();
                    case UINT:  return m_repr.b <=> other.to_bool();
                    case FLOAT: return m_repr.b <=> other.to_bool();
                    case STR:   return to_str() <=> other.m_repr.s.data();
                    default:      return std::partial_ordering::unordered;
                }
            }
            case INT: {
                switch (other.m_repr_ix) {
                    case BOOL:  return to_bool() <=> other.m_repr.b;
                    case INT:   return m_repr.i <=> other.m_repr.i;
                    case UINT:  {
                        return (other.m_repr.u > std::numeric_limits<Int>::max())?
                               std::partial_ordering::less:
                               m_repr.i <=> other.to_int();
                    }
                    case FLOAT: return m_repr.i <=> other.m_repr.f;
                    case STR:   return to_str() <=> other.m_repr.s.data();
                    default:      return std::partial_ordering::unordered;
                }
            }
            case UINT: {
                switch (other.m_repr_ix) {
                    case BOOL:  return to_bool() <=> other.m_repr.b;
                    case INT:   {
                        return (m_repr.u > std::numeric_limits<Int>::max())?
                               std::partial_ordering::greater:
                               to_int() <=> other.m_repr.i;
                    }
                    case UINT:  return m_repr.u <=> other.m_repr.u;
                    case FLOAT: return m_repr.u <=> other.m_repr.f;
                    case STR:   return to_str() <=> other.m_repr.s.data();
                    default:      return std::partial_ordering::unordered;
                }
            }
            case FLOAT: {
                switch (other.m_repr_ix) {
                    case BOOL:  return to_bool() <=> other.m_repr.b;
                    case INT:   return m_repr.f <=> other.m_repr.i;
                    case UINT:  return m_repr.f <=> other.m_repr.u;
                    case FLOAT: return m_repr.f <=> other.m_repr.f;
                    case STR:   return to_str() <=> other.m_repr.s.data();
                    default:      return std::partial_ordering::unordered;
                }
            }
            case STR: {
                return m_repr.s.data() <=> other.to_str();
            }
            default: {
                return std::partial_ordering::unordered;
            }
        }
    }

    ReprIX type() const   { return m_repr_ix; }
    auto type_name() const { return type_name(m_repr_ix); }

    template <typename T>
    bool is_type() const { return m_repr_ix == get_repr_ix<T>(); }

    bool is_any_int() const { return m_repr_ix == INT || m_repr_ix == UINT; }
    bool is_num() const     { return m_repr_ix >= INT && m_repr_ix <= FLOAT; }

    // unsafe, but will not segv
    template <typename T> T as() const requires is_byvalue<T>;
    template <> bool as<bool>() const     { return m_repr.b; }
    template <> Int as<Int>() const       { return m_repr.i; }
    template <> UInt as<UInt>() const     { return m_repr.u; }
    template <> Float as<Float>() const   { return m_repr.f; }

    template <typename T> const T& as() const requires std::is_same<T, StringView>::value {
        if (m_repr_ix != STR) throw wrong_type(m_repr_ix);
        return m_repr.s.data();
    }

    bool to_bool() const {
        switch (m_repr_ix) {
            case BOOL:  return m_repr.b;
            case INT:   return (bool)m_repr.i;
            case UINT:  return (bool)m_repr.u;
            case FLOAT: return (bool)m_repr.f;
            case STR:   return str_to_bool(m_repr.s.data());
            default: // TODO
                return std::numeric_limits<Int>::max();
        }
    }

    Int to_int() const {
        switch (m_repr_ix) {
            case BOOL:  return (Int)m_repr.b;
            case INT:   return m_repr.i;
            case UINT:  return (Int)m_repr.u;
            case FLOAT: return (Int)m_repr.f;
            case STR:   return str_to_int(m_repr.s.data());
            default: // TODO
                return std::numeric_limits<Int>::max();
        }
    }

    UInt to_uint() const {
        switch (m_repr_ix) {
            case BOOL:  return (UInt)m_repr.b;
            case INT:   return (UInt)m_repr.i;
            case UINT:  return m_repr.u;
            case FLOAT: return (UInt)m_repr.f;
            case STR:   return str_to_uint(m_repr.s.data());
            default:  // TODO
                return std::numeric_limits<UInt>::max();
        }
    }

    Float to_float() const {
        switch (m_repr_ix) {
            case BOOL:  return (Float)m_repr.b;
            case INT:   return (Float)m_repr.i;
            case UINT:  return (Float)m_repr.u;
            case FLOAT: return m_repr.f;
            case STR:   return str_to_float(m_repr.s.data());
            default:  // TODO
                return std::numeric_limits<Float>::max();
        }
    }

    void to_step(std::ostream& stream, bool is_first = false) const {
        switch (m_repr_ix) {
            case BOOL:  stream << (m_repr.b? "[1]": "[0]"); break;
            case INT:   stream << '[' << nodel::int_to_str(m_repr.i) << ']'; break;
            case UINT:  stream << '[' << nodel::int_to_str(m_repr.u) << ']'; break;
            case FLOAT: stream << '[' << nodel::float_to_str(m_repr.f) << ']'; break;
            case STR: {
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

    void to_str(std::ostream& stream) const {
        switch (m_repr_ix) {
            case NIL:  stream << "nil"; break;
            case BOOL:  stream << (m_repr.b? "true": "false"); break;
            case INT:   stream << nodel::int_to_str(m_repr.i); break;
            case UINT:  stream << nodel::int_to_str(m_repr.u); break;
            case FLOAT: stream << nodel::float_to_str(m_repr.f); break;
            case STR:   stream << m_repr.s.data(); break;
            default:      throw wrong_type(m_repr_ix);
        }
    }

    String to_str() const {
        std::stringstream ss;
        to_str(ss);
        return ss.str();
    }

    String to_json() const {
        switch (m_repr_ix) {
            case NIL:  return "nil";
            case BOOL:  return m_repr.b? "true": "false";
            case INT:   return nodel::int_to_str(m_repr.i);
            case UINT:  return nodel::int_to_str(m_repr.u);
            case FLOAT: {
                // IEEE 754-1985 - max digits=24
                std::string str(24, ' ');
                auto [ptr, err] = std::to_chars(str.data(), str.data() + str.size(), m_repr.f);
                ASSERT(err == std::errc());
                str.resize(ptr - str.data());
                return str;
            }
            case STR: {
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
            case NIL:  return 0;
            case BOOL:  return (size_t)m_repr.b;
            case INT:   return (size_t)m_repr.i;
            case UINT:  return (size_t)m_repr.u;
            case FLOAT: return (size_t)float_hash(m_repr.f);
            case STR:   return (size_t)m_repr.s.data().data();
            default:
                throw wrong_type(m_repr_ix);
        }
    }

    static WrongType wrong_type(ReprIX actual)                  { return type_name(actual); };
    static WrongType wrong_type(ReprIX actual, ReprIX expected) { return {type_name(actual), type_name(expected)}; };

  private:
    void release() {
        m_repr.z = nullptr;
        m_repr_ix = NIL;
    }

    template <typename T> ReprIX get_repr_ix() const;
    template <> ReprIX get_repr_ix<bool>() const     { return BOOL; }
    template <> ReprIX get_repr_ix<Int>() const      { return INT; }
    template <> ReprIX get_repr_ix<UInt>() const     { return UINT; }
    template <> ReprIX get_repr_ix<Float>() const    { return FLOAT; }
    template <> ReprIX get_repr_ix<intern_t>() const { return STR; }
    template <> ReprIX get_repr_ix<String>() const   { return STR; }

  private:
    Repr m_repr;
    ReprIX m_repr_ix;

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
        case Key::NIL:  return ostream << "nil";
        case Key::BOOL:  return ostream << key.m_repr.b;
        case Key::INT:   return ostream << key.m_repr.i;
        case Key::UINT:  return ostream << key.m_repr.u;
        case Key::FLOAT: return ostream << key.m_repr.f;
        case Key::STR:   return ostream << key.m_repr.s.data();
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
//template <> bool is_bool(const Key& key)  { return key.is_type<bool>(); }
//template <> bool is_int(const Key& key)   { return key.is_type<Int>(); }
//template <> bool is_uint(const Key& key)  { return key.is_type<UInt>(); }
//template <> bool is_float(const Key& key) { return key.is_type<Float>(); }
//template <> bool is_str(const Key& key)   { return key.is_type<String>(); }
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
