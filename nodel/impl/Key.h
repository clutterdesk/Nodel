#pragma once

#include "support.h"
#include "types.h"

#include <string>
#include <charconv>
#include <iomanip>

using namespace std::literals::string_literals;
using namespace std::literals::string_view_literals;

namespace nodel {

struct WrongType : std::exception
{
    WrongType(const std::string_view& actual) {
        std::stringstream ss;
        ss << "type=" << actual;
        msg = ss.str();
    }

    WrongType(const std::string_view& actual, const std::string_view& expected) {
        std::stringstream ss;
        ss << "type=" << actual << ", expected=" << expected;
        msg = ss.str();
    }

    const char* what() const noexcept override { return msg.c_str(); }
    std::string msg;
};


class Key
{
  private:
    enum {
        EMPTY_I,  // reserved for congruence with Object enum
        NULL_I,   // json null
        BOOL_I,
        INT_I,
        UINT_I,
        FLOAT_I,
        STR_I,
        STRV_I
    };

    union Repr {
        Repr()                           : z{(void*)0} {}
        Repr(bool v)                     : b{v} {}
        Repr(Int v)                      : i{v} {}
        Repr(UInt v)                     : u{v} {}
        Repr(Float v)                    : f{v} {}
        Repr(std::string* p)             : p_s{p} {}
        Repr(const std::string_view& sv) : sv{sv} {}

        void* z;
        bool  b;
        Int   i;
        UInt  u;
        Float f;
        std::string* p_s;
        std::string_view sv;
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
            case STRV_I:   return "string_view";
            default:      throw std::logic_error("invalid repr_ix");
        }
    }

  public:
    Key()                           : m_repr{}, m_repr_ix{NULL_I} {}
    Key(bool v)                     : m_repr{v}, m_repr_ix{BOOL_I} {}
    Key(is_like_Float auto v)       : m_repr{v}, m_repr_ix{FLOAT_I} {}
    Key(is_like_Int auto v)         : m_repr{(Int)v}, m_repr_ix{INT_I} {}
    Key(is_like_UInt auto v)        : m_repr{(UInt)v}, m_repr_ix{UINT_I} {}
    Key(const std::string_view& sv) : m_repr{sv}, m_repr_ix{STRV_I} {}                                  // intended for string literals
    Key(std::string_view&& sv)      : m_repr{std::forward<std::string_view>(sv)}, m_repr_ix{STRV_I} {}  // intended for string literals
    Key(const std::string& s)       : m_repr{new std::string(s)}, m_repr_ix{STR_I} {}
    Key(std::string&& s)            : m_repr{new std::string(std::forward<std::string>(s))}, m_repr_ix{STR_I} {}
    Key(const char* s)              : m_repr{new std::string(s)}, m_repr_ix{STR_I} {}  // slow, use std::literals::string_view_literals

    ~Key() { release(); }

    Key(const Key& key) {
        m_repr_ix = key.m_repr_ix;
        switch (m_repr_ix) {
            case NULL_I:  m_repr.z = key.m_repr.z; break;
            case BOOL_I:  m_repr.b = key.m_repr.b; break;
            case INT_I:   m_repr.i = key.m_repr.i; break;
            case UINT_I:  m_repr.u = key.m_repr.u; break;
            case FLOAT_I: m_repr.f = key.m_repr.f; break;
            case STR_I:   m_repr.p_s = new std::string{*(key.m_repr.p_s)}; break;
            case STRV_I:  m_repr.sv = key.m_repr.sv; break;
        }
    }

    Key(Key&& key) {
        m_repr_ix = key.m_repr_ix;
        if (m_repr_ix == STRV_I) {
            m_repr.sv = key.m_repr.sv;
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
            case STR_I:   m_repr.p_s = new std::string{*(key.m_repr.p_s)}; break;
            case STRV_I:  m_repr.sv = key.m_repr.sv; break;
        }
        return *this;
    }

    Key& operator = (Key&& key) {
        release();
        m_repr_ix = key.m_repr_ix;
        switch (m_repr_ix) {
            case NULL_I:  m_repr.z = key.m_repr.z; break;
            case BOOL_I:  m_repr.b = key.m_repr.b; break;
            case INT_I:   m_repr.i = key.m_repr.i; break;
            case UINT_I:  m_repr.u = key.m_repr.u; break;
            case FLOAT_I: m_repr.f = key.m_repr.f; break;
            case STR_I:
                m_repr.p_s = key.m_repr.p_s;
                key.m_repr.z = (void*)0;
                key.m_repr_ix = NULL_I;
                break;
            case STRV_I:  m_repr.sv = m_repr.sv; break;
        }
        return *this;
    }

    Key& operator = (std::nullptr_t)       { release(); m_repr.z = nullptr; m_repr_ix = NULL_I; return *this; }
    Key& operator = (bool v)               { release(); m_repr.b = v; m_repr_ix = BOOL_I; return *this; }
    Key& operator = (is_like_Int auto v)   { release(); m_repr.i = v; m_repr_ix = INT_I; return *this; }
    Key& operator = (is_like_UInt auto v)  { release(); m_repr.u = v; m_repr_ix = UINT_I; return *this; }
    Key& operator = (Float v)              { release(); m_repr.f = v; m_repr_ix = FLOAT_I; return *this; }

    Key& operator = (const std::string& s) {
        release();
        m_repr.p_s = new std::string{s};
        m_repr_ix = STR_I;
        return *this;
    }

    Key& operator = (std::string&& s) {
        release();
        m_repr.p_s = new std::string{std::forward<std::string>(s)};
        m_repr_ix = STR_I;
        return *this;
    }

    Key& operator = (const std::string_view& sv) {
        release();
        m_repr.sv = sv;
        m_repr_ix = STRV_I;
        return *this;
    }

    Key& operator = (std::string_view&& sv) {
        release();
        m_repr.sv = std::forward<std::string_view>(sv);
        m_repr_ix = STRV_I;
        return *this;
    }

    Key& operator = (const char* s) { return operator = (std::string{s}); }  // slow, use std::literals::string_view_literals

    bool operator == (const Key& other) const {
        if (m_repr_ix == other.m_repr_ix) {
            switch (m_repr_ix) {
                case NULL_I:  return true;
                case BOOL_I:  return m_repr.b == other.m_repr.b;
                case INT_I:   return m_repr.i == other.m_repr.i;
                case UINT_I:  return m_repr.u == other.m_repr.u;
                case FLOAT_I: return m_repr.f == other.m_repr.f;
                case STR_I:   return *(m_repr.p_s) == *(other.m_repr.p_s);
                case STRV_I:  return m_repr.sv == other.m_repr.sv;
            }
        } else {
            switch (m_repr_ix) {
                case NULL_I:  return false;
                case BOOL_I:  return m_repr.b == other.to_bool();
                case INT_I:   return m_repr.i == other.to_int();
                case UINT_I:  return m_repr.u == other.to_uint();
                case FLOAT_I: return m_repr.f == other.to_float();
                case STR_I: {
                    switch (other.m_repr_ix) {
                        case STR_I:  return *(m_repr.p_s) == *(other.m_repr.p_s);
                        case STRV_I: return *(m_repr.p_s) == other.m_repr.sv;
                        default:
                            return false;
                    }
                }
                case STRV_I: {
                    switch (other.m_repr_ix) {
                        case STR_I:  return m_repr.sv == *(other.m_repr.p_s);
                        case STRV_I: return m_repr.sv == other.m_repr.sv;
                        default:
                            return false;
                    }
                }
            }
        }
        return false;
    }

    bool operator == (const std::string& other) const {
        switch (m_repr_ix) {
            case NULL_I: return other == "null";
            case STR_I:  return *(m_repr.p_s) == other;
            case STRV_I: return m_repr.sv == other;
            default:
                break;
        }
        return false;
    }

    bool operator == (const std::string_view& other) const {
        switch (m_repr_ix) {
            case NULL_I: return other == "null";
            case STR_I:  return *(m_repr.p_s) == other;
            case STRV_I: return m_repr.sv == other;
            default:
                break;
        }
        return false;
    }

    bool operator == (const char *other) const {
        return operator == (std::string_view{other});
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

    bool is_null() const    { return m_repr_ix == NULL_I; }
    bool is_bool() const    { return m_repr_ix == BOOL_I; }
    bool is_int() const     { return m_repr_ix == INT_I; }
    bool is_uint() const    { return m_repr_ix == UINT_I; }
    bool is_any_int() const { return m_repr_ix == INT_I || m_repr_ix == UINT_I; }
    bool is_float() const   { return m_repr_ix == FLOAT_I; }
    bool is_str() const     { return m_repr_ix >= STR_I; }
    bool is_num() const     { return m_repr_ix && m_repr_ix < STR_I; }

    // unsafe
    bool as_bool() const                        { return m_repr.b; }
    Int as_int() const                          { return m_repr.i; }
    UInt as_uint() const                        { return m_repr.u; }
    Float as_float() const                      { return m_repr.f; }
    const std::string_view as_str() const      {
        return (m_repr_ix == STR_I)? (std::string_view)*m_repr.p_s: m_repr.sv;
    }

    bool to_bool() const {
        switch (m_repr_ix) {
            case BOOL_I:  return m_repr.b;
            case INT_I:   return (bool)m_repr.i;
            case UINT_I:  return (bool)m_repr.u;
            case FLOAT_I: return (bool)m_repr.f;
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
            default:  // TODO
                return std::numeric_limits<UInt>::max();
        }
    }

    void to_step(std::ostream& stream) const {
        switch (m_repr_ix) {
            case BOOL_I:  stream << (m_repr.b? "[1]": "[0]"); break;
            case INT_I:   stream << '[' << nodel::int_to_str(m_repr.i) << ']'; break;
            case UINT_I:  stream << '[' << nodel::int_to_str(m_repr.u) << ']'; break;
            case FLOAT_I: stream << '[' << nodel::float_to_str(m_repr.f) << ']'; break;
            case STR_I: {
                make_path_step((std::string_view)(*(m_repr.p_s)), stream);
                break;
            }
            case STRV_I: {
                make_path_step(m_repr.sv, stream);
                break;
            }
            default:
                throw wrong_type(m_repr_ix);
        }
    }

    std::string to_str() const {
        switch (m_repr_ix) {
            case NULL_I:  return "null";
            case BOOL_I:  return m_repr.b? "true": "false";
            case INT_I:   return nodel::int_to_str(m_repr.i);
            case UINT_I:  return nodel::int_to_str(m_repr.u);
            case FLOAT_I: return nodel::float_to_str(m_repr.f);
            case STR_I:   return *(m_repr.p_s);
            case STRV_I:  return std::string(m_repr.sv);
            default:      throw wrong_type(m_repr_ix);
        }
    }

    std::string to_json() const {
        switch (m_repr_ix) {
            case NULL_I:  return "null";
            case BOOL_I:  return m_repr.b? "true": "false";
            case INT_I:   return nodel::int_to_str(m_repr.i);
            case UINT_I:  return nodel::int_to_str(m_repr.u);
            case FLOAT_I: {
                // IEEE 754-1985 - max digits=24
                std::string str(24, ' ');
                auto [ptr, err] = std::to_chars(str.data(), str.data() + str.size(), m_repr.f);
                assert(err == std::errc());
                str.resize(ptr - str.data());
                return str;
            }
            case STR_I: {
                std::stringstream ss;
                ss << std::quoted(*(m_repr.p_s));
                return ss.str();
            }
            case STRV_I: {
                std::stringstream ss;
                ss << std::quoted(m_repr.sv);
                return ss.str();
            }
            default:      return "?";
        }
    }

    size_t hash() const {
        static std::hash<Float> float_hash;
        static std::hash<std::string_view> string_hash;
        switch (m_repr_ix) {
            case NULL_I:  return 0;
            case BOOL_I:  return (size_t)m_repr.b;
            case INT_I:   return (size_t)m_repr.i;
            case UINT_I:  return (size_t)m_repr.u;
            case FLOAT_I: return (size_t)float_hash(m_repr.f);
            case STR_I:   return string_hash((std::string_view)(*(m_repr.p_s)));
            case STRV_I:  return string_hash(m_repr.sv);
            default:      return 0;
        }
    }

  private:
    void release() {
        if (m_repr_ix == STR_I) delete m_repr.p_s;
        m_repr.z = nullptr;
        m_repr_ix = NULL_I;
    }

    WrongType wrong_type(uint8_t actual) const                   { return type_name(actual); };
    WrongType wrong_type(uint8_t actual, uint8_t expected) const { return {type_name(actual), type_name(expected)}; };

  private:
    Repr m_repr;
    uint8_t m_repr_ix;

    friend std::ostream& operator<< (std::ostream& ostream, const Key& key);
    friend class Object;
};


inline
std::ostream& operator<< (std::ostream& ostream, const Key& key) {
    switch (key.m_repr_ix) {
        case Key::NULL_I:  return ostream << "null";
        case Key::BOOL_I:  return ostream << key.m_repr.b;
        case Key::INT_I:   return ostream << key.m_repr.i;
        case Key::UINT_I:  return ostream << key.m_repr.u;
        case Key::FLOAT_I: return ostream << key.m_repr.f;
        case Key::STR_I:   return ostream << *key.m_repr.p_s;
        case Key::STRV_I:  return ostream << key.m_repr.sv;
        default:           throw std::invalid_argument("key");
    }
}

struct KeyHash
{
    size_t operator () (const Key& key) const {
        return key.hash();
    }
};


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
