#pragma once

#include "str_util.h"
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
    Key()                           : repr{}, repr_ix{NULL_I} {}
    Key(bool v)                     : repr{v}, repr_ix{BOOL_I} {}
    Key(is_like_Float auto v)       : repr{v}, repr_ix{FLOAT_I} {}
    Key(is_like_Int auto v)         : repr{(Int)v}, repr_ix{INT_I} {}
    Key(is_like_UInt auto v)        : repr{(UInt)v}, repr_ix{UINT_I} {}
    Key(const std::string_view& sv) : repr{sv}, repr_ix{STRV_I} {}                                  // intended for string literals
    Key(std::string_view&& sv)      : repr{std::forward<std::string_view>(sv)}, repr_ix{STRV_I} {}  // intended for string literals
    Key(const std::string& s)       : repr{new std::string(s)}, repr_ix{STR_I} {}
    Key(std::string&& s)            : repr{new std::string(std::forward<std::string>(s))}, repr_ix{STR_I} {}
    Key(const char* s)              : repr{new std::string(s)}, repr_ix{STR_I} {}  // slow, use std::literals::string_view_literals

    ~Key() { free(); }

    Key(const Key& key) {
        repr_ix = key.repr_ix;
        switch (key.repr_ix) {
            case NULL_I:  repr.z = key.repr.z; break;
            case BOOL_I:  repr.b = key.repr.b; break;
            case INT_I:   repr.i = key.repr.i; break;
            case UINT_I:  repr.u = key.repr.u; break;
            case FLOAT_I: repr.f = key.repr.f; break;
            case STR_I:   repr.p_s = new std::string{*(key.repr.p_s)}; break;
            case STRV_I:  repr.sv = repr.sv; break;
        }
    }

    Key(Key&& key) {
        repr_ix = key.repr_ix;
        if (repr_ix == STRV_I) {
            repr.sv = key.repr.sv;
        } else {
            // generic memory copy
            repr.u = key.repr.u;
        }
        key.repr.z = (void*)0;
        key.repr_ix = NULL_I;
    }

    Key& operator = (const Key& key) {
        free();
        repr_ix = key.repr_ix;
        switch (repr_ix) {
            case NULL_I:  repr.z = key.repr.z; break;
            case BOOL_I:  repr.b = key.repr.b; break;
            case INT_I:   repr.i = key.repr.i; break;
            case UINT_I:  repr.u = key.repr.u; break;
            case FLOAT_I: repr.f = key.repr.f; break;
            case STR_I:   repr.p_s = new std::string{*(key.repr.p_s)}; break;
            case STRV_I:  repr.sv = key.repr.sv; break;
        }
        return *this;
    }

    Key& operator = (Key&& key) {
        free();
        repr_ix = key.repr_ix;
        switch (repr_ix) {
            case NULL_I:  repr.z = key.repr.z; break;
            case BOOL_I:  repr.b = key.repr.b; break;
            case INT_I:   repr.i = key.repr.i; break;
            case UINT_I:  repr.u = key.repr.u; break;
            case FLOAT_I: repr.f = key.repr.f; break;
            case STR_I:
                repr.p_s = key.repr.p_s;
                key.repr.z = (void*)0;
                key.repr_ix = NULL_I;
                break;
            case STRV_I:  repr.sv = repr.sv; break;
        }
        return *this;
    }

    Key& operator = (std::nullptr_t)       { free(); repr.z = nullptr; repr_ix = NULL_I; return *this; }
    Key& operator = (bool v)               { free(); repr.b = v; repr_ix = BOOL_I; return *this; }
    Key& operator = (is_like_Int auto v)   { free(); repr.i = v; repr_ix = INT_I; return *this; }
    Key& operator = (is_like_UInt auto v)  { free(); repr.u = v; repr_ix = UINT_I; return *this; }
    Key& operator = (Float v)              { free(); repr.f = v; repr_ix = FLOAT_I; return *this; }

    Key& operator = (const std::string& s) {
        free();
        repr.p_s = new std::string{s};
        repr_ix = STR_I;
        return *this;
    }

    Key& operator = (std::string&& s) {
        free();
        repr.p_s = new std::string{std::forward<std::string>(s)};
        repr_ix = STR_I;
        return *this;
    }

    Key& operator = (const std::string_view& sv) {
        free();
        repr.sv = sv;
        repr_ix = STRV_I;
        return *this;
    }

    Key& operator = (std::string_view&& sv) {
        free();
        repr.sv = std::forward<std::string_view>(sv);
        repr_ix = STRV_I;
        return *this;
    }

    Key& operator = (const char* s) { return operator = (std::string{s}); }  // slow, use std::literals::string_view_literals

    bool operator == (const Key& other) const {
        if (repr_ix == other.repr_ix) {
            switch (repr_ix) {
                case NULL_I:  return true;
                case BOOL_I:  return repr.b == other.repr.b;
                case INT_I:   return repr.i == other.repr.i;
                case UINT_I:  return repr.u == other.repr.u;
                case FLOAT_I: return repr.f == other.repr.f;
                case STR_I:   return *(repr.p_s) == *(other.repr.p_s);
                case STRV_I:  return repr.sv == other.repr.sv;
            }
        } else {
            switch (repr_ix) {
                case NULL_I:  return false;
                case BOOL_I:  return repr.b == other.to_bool();
                case INT_I:   return repr.i == other.to_int();
                case UINT_I:  return repr.u == other.to_uint();
                case FLOAT_I: return repr.f == other.to_float();
                case STR_I: {
                    switch (other.repr_ix) {
                        case STR_I:  return *(repr.p_s) == *(other.repr.p_s);
                        case STRV_I: return *(repr.p_s) == other.repr.sv;
                        default:
                            return false;
                    }
                }
                case STRV_I: {
                    switch (other.repr_ix) {
                        case STR_I:  return repr.sv == *(other.repr.p_s);
                        case STRV_I: return repr.sv == other.repr.sv;
                        default:
                            return false;
                    }
                }
            }
        }
        return false;
    }

    bool operator == (const std::string& other) const {
        if (repr_ix == STR_I) {
            return *(repr.p_s) == other;
        } else if (repr_ix == STRV_I) {
            return repr.sv == other;
        } else {
            return false;
        }
    }

    bool operator == (const std::string_view& other) const {
        if (repr_ix == STR_I) {
            return *(repr.p_s) == other;
        } else if (repr_ix == STRV_I) {
            return repr.sv == other;
        } else {
            return false;
        }
    }

    bool operator == (const char *other) const {
        return operator == (std::string_view{other});
    }

    bool operator == (is_number auto other) const {
        switch (repr_ix) {
            case BOOL_I:  return repr.b == other;
            case INT_I:   return repr.i == other;
            case UINT_I:  return repr.u == other;
            case FLOAT_I: return repr.f == other;
            default:
                return false;
        }
    }

    bool is_null() const  { return repr_ix == NULL_I; }
    bool is_bool() const  { return repr_ix == BOOL_I; }
    bool is_int() const   { return repr_ix == INT_I; }
    bool is_uint() const  { return repr_ix == UINT_I; }
    bool is_float() const { return repr_ix == FLOAT_I; }
    bool is_str() const   { return repr_ix >= STR_I; }
    bool is_num() const   { return repr_ix && repr_ix < STR_I; }

    // unsafe
    bool as_bool() const                        { return repr.b; }
    Int as_int() const                          { return repr.i; }
    UInt as_uint() const                        { return repr.u; }
    Float as_float() const                      { return repr.f; }
    const std::string_view as_str() const      {
        return (repr_ix == STR_I)? (std::string_view)*repr.p_s: repr.sv;
    }

    bool to_bool() const {
        switch (repr_ix) {
            case BOOL_I:  return repr.b;
            case INT_I:   return (bool)repr.i;
            case UINT_I:  return (bool)repr.u;
            case FLOAT_I: return (bool)repr.f;
            default:
                return std::numeric_limits<Int>::max();
        }
    }

    Int to_int() const {
        switch (repr_ix) {
            case BOOL_I:  return (Int)repr.b;
            case INT_I:   return repr.i;
            case UINT_I:  return (Int)repr.u;
            case FLOAT_I: return (Int)repr.f;
            default:
                return std::numeric_limits<Int>::max();
        }
    }

    UInt to_uint() const {
        switch (repr_ix) {
            case BOOL_I:  return (UInt)repr.b;
            case INT_I:   return (UInt)repr.i;
            case UINT_I:  return repr.u;
            case FLOAT_I: return (UInt)repr.f;
            default:
                return std::numeric_limits<UInt>::max();
        }
    }

    Float to_float() const {
        switch (repr_ix) {
            case BOOL_I:  return (Float)repr.b;
            case INT_I:   return (Float)repr.i;
            case UINT_I:  return (Float)repr.u;
            case FLOAT_I: return repr.f;
            default:
                return std::numeric_limits<UInt>::max();
        }
    }

    std::string to_str() const {
        switch (repr_ix) {
            case NULL_I:  return "null";
            case BOOL_I:  return repr.b? "true": "false";
            case INT_I:   return nodel::int_to_str(repr.i);
            case UINT_I:  return nodel::int_to_str(repr.u);
            case FLOAT_I: return nodel::float_to_str(repr.f);
            case STR_I:  return *(repr.p_s);
            case STRV_I: return std::string(repr.sv);
            default:     return "?";
        }
    }

    std::string to_json() const {
        switch (repr_ix) {
            case NULL_I:  return "null";
            case BOOL_I:  return repr.b? "true": "false";
            case INT_I:   return nodel::int_to_str(repr.i);
            case UINT_I:  return nodel::int_to_str(repr.u);
            case FLOAT_I: {
                // IEEE 754-1985 - max digits=24
                std::string str(24, ' ');
                auto [ptr, err] = std::to_chars(str.data(), str.data() + str.size(), repr.f);
                assert(err == std::errc());
                str.resize(ptr - str.data());
                return str;
            }
            case STR_I: {
                std::stringstream ss;
                ss << std::quoted(*(repr.p_s));
                return ss.str();
            }
            case STRV_I: {
                std::stringstream ss;
                ss << std::quoted(repr.sv);
                return ss.str();
            }
            default:      return "?";
        }
    }

    size_t hash() const {
        static std::hash<Float> float_hash;
        static std::hash<std::string_view> string_hash;
        switch (repr_ix) {
            case NULL_I:  return 0;
            case BOOL_I:  return (size_t)repr.b;
            case INT_I:   return (size_t)repr.i;
            case UINT_I:  return (size_t)repr.u;
            case FLOAT_I: return (size_t)float_hash(repr.f);
            case STR_I:   return string_hash((std::string_view)(*(repr.p_s)));
            case STRV_I:  return string_hash(repr.sv.data());
            default:      return 0;
        }
    }

  private:
    void free() {
        if (repr_ix == STR_I) delete repr.p_s;
        repr.z = nullptr;
        repr_ix = NULL_I;
    }

    WrongType wrong_type(uint8_t actual) const                   { return type_name(actual); };
    WrongType wrong_type(uint8_t actual, uint8_t expected) const { return {type_name(actual), type_name(expected)}; };

  private:
    Repr repr;
    uint8_t repr_ix;

    friend std::ostream& operator<< (std::ostream& ostream, const Key& key);
    friend class Object;
};


inline
std::ostream& operator<< (std::ostream& ostream, const Key& key) {
    switch (key.repr_ix) {
        case Key::NULL_I:  return ostream << "null";
        case Key::BOOL_I:  return ostream << key.repr.b;
        case Key::INT_I:   return ostream << key.repr.i;
        case Key::UINT_I:  return ostream << key.repr.u;
        case Key::FLOAT_I: return ostream << key.repr.f;
        case Key::STR_I:   return ostream << *key.repr.p_s;
        case Key::STRV_I:  return ostream << key.repr.sv;
        default:      throw std::invalid_argument("key");
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
