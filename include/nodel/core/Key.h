/** @file */
#pragma once

#include <string>
#include <cstring>
#include <charconv>
#include <iomanip>
#include <ctime>

#include <nodel/support/logging.h>
#include <nodel/support/string.h>
#include <nodel/support/integer.h>
#include <nodel/support/intern.h>
#include <nodel/support/exception.h>
#include <nodel/support/types.h>

using namespace std::literals::string_literals;
using namespace std::literals::string_view_literals;

/// Nodel namespace
namespace nodel {

/////////////////////////////////////////////////////////////////////////////
/// A class to represent keys in a dictionary or list.
/// - The Key class is a dynamic type like the Object class. However, it only
///   supports the following data types:
///     - nil
///     - boolean
///     - integer          (64-bit, as defined in nodel/support/types.h)
///     - unsigned integer (64-bit, as defined in nodel/support/types.h)
///     - floating point   (64-bit, as defined in nodel/support/types.h)
///     - string           (may represent either text, or binary data)
/// - String keys are interned (see nodel/support/intern.h).
/// - A literal operator `""_key` is provided, which is fast to create because
///   it is known that the data is a read-only literal.
/// - String interning has several benefits including:
///     - String keys can be compared by comparing pointers
///     - A string key can be hashed by hashing its pointer
///     - A string key copy only has to assign a pointer
/// - When creating Key instances from non-literal strings, the string must
///   first be interned at the cost of a hash table lookup and comparison.
/// - Applications with a high thread-count, and or a large/unbounded string
///   key domain, may see significant overhead from the per-thread intern
///   tables. This can be addressed in the future.
/////////////////////////////////////////////////////////////////////////////
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
            case NIL:   return "nil";
            case BOOL:  return "bool";
            case INT:   return "int";
            case UINT:  return "uint";
            case FLOAT: return "float";
            case STR:   return "string";
            default:    throw std::logic_error("invalid repr_ix");
        }
    }

  public:
    Key()                     : m_repr{}, m_repr_ix{NIL} {}
    Key(nil_t)                : m_repr{}, m_repr_ix{NIL} {}
    Key(const String& s)      : m_repr{intern_string(s)}, m_repr_ix{STR} {}
    Key(const StringView& s)  : m_repr{intern_string(s)}, m_repr_ix{STR} {}
    Key(is_bool auto v)       : m_repr{v}, m_repr_ix{BOOL} {}
    Key(is_like_Float auto v) : m_repr{v}, m_repr_ix{FLOAT} {}
    Key(is_like_Int auto v)   : m_repr{(Int)v}, m_repr_ix{INT} {}
    Key(is_like_UInt auto v)  : m_repr{(UInt)v}, m_repr_ix{UINT} {}
    Key(intern_t s)           : m_repr{s}, m_repr_ix{STR} {}

    ~Key() { release(); }

    Key(const Key& key) {
        m_repr_ix = key.m_repr_ix;
        switch (m_repr_ix) {
            case NIL:   m_repr.z = key.m_repr.z; break;
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
            case NIL:   m_repr.z = key.m_repr.z; break;
            case BOOL:  m_repr.b = key.m_repr.b; break;
            case INT:   m_repr.i = key.m_repr.i; break;
            case UINT:  m_repr.u = key.m_repr.u; break;
            case FLOAT: m_repr.f = key.m_repr.f; break;
            case STR:   m_repr.s = key.m_repr.s; break;
        }
        return *this;
    }

    Key& operator = (nil_t)                { release(); m_repr.z = nullptr; m_repr_ix = NIL; return *this; }
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

    bool operator == (is_bool auto other) const {
        switch (m_repr_ix) {
            case BOOL:  return m_repr.b == other;
            case INT:   return (bool)m_repr.i == other;
            case UINT:  return (bool)m_repr.u == other;
            case FLOAT: return (bool)m_repr.f == other;
            default:    return false;
        }
    }

    bool operator == (is_like_Int auto other) const {
        switch (m_repr_ix) {
            case BOOL:  return m_repr.b == (bool)other;
            case INT:   return m_repr.i == other;
            case UINT:  return equal(m_repr.u, other);
            case FLOAT: return m_repr.f == other;
            default:    return false;
        }
    }

    bool operator == (is_like_UInt auto other) const {
        switch (m_repr_ix) {
            case BOOL:  return m_repr.b == (bool)other;
            case INT:   return equal(other, m_repr.i);
            case UINT:  return m_repr.u == other;
            case FLOAT: return m_repr.f == other;
            default:    return false;
        }
    }

    bool operator == (Float other) const {
        switch (m_repr_ix) {
            case BOOL:  return m_repr.b == (bool)other;
            case INT:   return (Float)m_repr.i == other;
            case UINT:  return (Float)m_repr.u == other;
            case FLOAT: return m_repr.f == other;
            default:    return false;
        }
    }

    bool operator == (const Key& other) const {
        switch (m_repr_ix) {
            case NIL:   return other == nil;
            case BOOL:  return other == m_repr.b;
            case INT:   return other == m_repr.i;
            case UINT:  return other == m_repr.u;
            case FLOAT: return other == m_repr.f;
            case STR:   return other == m_repr.s;
            default:    break;
        }
        return false;
    }

    std::partial_ordering operator <=> (const Key& other) const {
        switch (m_repr_ix) {
            case BOOL: {
                switch (other.m_repr_ix) {
                    case BOOL:  return m_repr.b <=> other.m_repr.b;
                    case INT:   return (Int)m_repr.b <=> other.m_repr.i;
                    case UINT:  return (UInt)m_repr.b <=> other.m_repr.u;
                    case FLOAT: return (Float)m_repr.b <=> other.m_repr.f;
                    case STR:   return std::partial_ordering::less;
                    default:    return std::partial_ordering::unordered;
                }
            }
            case INT: {
                switch (other.m_repr_ix) {
                    case BOOL:  return m_repr.i <=> other.to_int();
                    case INT:   return m_repr.i <=> other.m_repr.i;
                    case UINT:  return compare(m_repr.i, other.m_repr.u);
                    case FLOAT: return m_repr.i <=> other.m_repr.f;
                    case STR:   return std::partial_ordering::less;
                    default:    return std::partial_ordering::unordered;
                }
            }
            case UINT: {
                switch (other.m_repr_ix) {
                    case BOOL:  return m_repr.u <=> other.to_uint();
                    case INT:   return compare(m_repr.u, other.m_repr.i);
                    case UINT:  return m_repr.u <=> other.m_repr.u;
                    case FLOAT: return m_repr.u <=> other.m_repr.f;
                    case STR:   return std::partial_ordering::less;
                    default:    return std::partial_ordering::unordered;
                }
            }
            case FLOAT: {
                switch (other.m_repr_ix) {
                    case BOOL:  return m_repr.f <=> other.to_float();
                    case INT:   return m_repr.f <=> other.m_repr.i;
                    case UINT:  return m_repr.f <=> other.m_repr.u;
                    case FLOAT: return m_repr.f <=> other.m_repr.f;
                    case STR:   return std::partial_ordering::less;
                    default:    return std::partial_ordering::unordered;
                }
            }
            case STR: {
                switch (other.m_repr_ix) {
                    case BOOL:  return std::partial_ordering::greater;
                    case INT:   return std::partial_ordering::greater;
                    case UINT:  return std::partial_ordering::greater;
                    case FLOAT: return std::partial_ordering::greater;
                    case STR:   return m_repr.s.data() <=> other.m_repr.s.data();
                    default:    return std::partial_ordering::unordered;
                }
            }
            default: {
                return std::partial_ordering::unordered;
            }
        }
    }

    ReprIX type() const   { return m_repr_ix; }
    auto type_name() const { return type_name(m_repr_ix); }

    /// Returns true if the Key contains the data type, T
    /// @tparam T One of the data types defined in the Repr union.
    template <typename T>
    bool is_type() const { return m_repr_ix == get_repr_ix<T>(); }

    /// Returns true if the Key is an INT or UINT
    bool is_any_int() const { return m_repr_ix == INT || m_repr_ix == UINT; }

    /// Returns true if the Key is a INT, UINT, or FLOAT
    bool is_num() const     { return m_repr_ix >= INT && m_repr_ix <= FLOAT; }

    /// Unchecked access to the the backing data
    /// @tparam T One of the data types defined in the Repr union.
    template <typename T> T as() const requires is_byvalue<T>;
    template <> bool as<bool>() const     { return m_repr.b; }
    template <> Int as<Int>() const       { return m_repr.i; }
    template <> UInt as<UInt>() const     { return m_repr.u; }
    template <> Float as<Float>() const   { return m_repr.f; }

    template <typename T> const T& as() const requires std::is_same<T, StringView>::value {
        if (m_repr_ix != STR) throw wrong_type(m_repr_ix);
        return m_repr.s.data();
    }

    /// Convert the backing data to a boolean.
    /// Numeric types are converted via C++ cast to bool.
    /// String data is converted by calling `nodel::str_to_bool` (see nodel/support/string.h).
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

    /// Convert the backing data to a signed integer.
    /// Numeric types are converted via C++ cast to Int.
    /// String data is converted by calling `nodel::str_to_int` (see nodel/support/string.h).
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

    /// Convert the backing data to an unsigned integer.
    /// Numeric types are converted via C++ cast to UInt.
    /// String data is converted by calling `nodel::str_to_uint` (see nodel/support/string.h).
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

    /// Convert the backing data to floating point.
    /// Numeric types are converted via C++ cast to Float.
    /// String data is converted by calling `nodel::str_to_float` (see nodel/support/string.h).
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

    /// Convert a Key to a string representation of a nodel::OPath step.
    /// @param stream The output stream where the string is written.
    /// @param is_first Should be true for the first step in a path.
    /// - A path step converted with this function can be deserialize by the
    ///   OPath class.
    /// - A boolean Key is converted to `[0]` or `[1]`.
    /// - An integer Key is converted to a string of the form, `[<integer>]`.
    /// - A float key is converted to a string of the form, `[<float>]`.
    /// - If `is_first` is true, a string key that does not contain a
    ///   double-quote character is converted to a string of the form,
    ///   `<string>`.
    /// - If `is_first` is false, a string key that does not contain a
    ///   double-quote character is converted to a string of the form,
    ///   `.<string>`.
    /// - A string key that contains a double-quote character is converted
    ///   to a string of the form, `["<string>"]`.
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

    /// Convert the Key data to a string and write to an output stream.
    /// @param stream The output stream where the string will be written.
    void to_str(std::ostream& stream) const {
        switch (m_repr_ix) {
            case NIL:   stream << "nil"; break;
            case BOOL:  stream << (m_repr.b? "true": "false"); break;
            case INT:   stream << nodel::int_to_str(m_repr.i); break;
            case UINT:  stream << nodel::int_to_str(m_repr.u); break;
            case FLOAT: stream << nodel::float_to_str(m_repr.f); break;
            case STR:   stream << m_repr.s.data(); break;
            default:    throw wrong_type(m_repr_ix);
        }
    }

    /// Convert the Key data to a string.
    String to_str() const {
        switch (m_repr_ix) {
            case NIL:   return "nil";
            case BOOL:  return (m_repr.b? "true": "false");
            case INT:   return nodel::int_to_str(m_repr.i);
            case UINT:  return nodel::int_to_str(m_repr.u);
            case FLOAT: return nodel::float_to_str(m_repr.f);
            case STR: {
                auto& str = m_repr.s.data();
                return {str.data(), str.size()};
            }
            default:
                throw wrong_type(m_repr_ix);
        }
    }

    /// Convert the Key data to a JSON string.
    String to_json() const {
        switch (m_repr_ix) {
            case NIL:  return "nil";
            case BOOL:  return m_repr.b? "true": "false";
            case INT:   return nodel::int_to_str(m_repr.i);
            case UINT:  return nodel::int_to_str(m_repr.u);
            case FLOAT: return nodel::float_to_str(m_repr.f);
            case STR: {
                StringStream ss;
                ss << std::quoted(m_repr.s.data());
                return ss.str();
            }
            default:
                throw wrong_type(m_repr_ix);
        }
    }

    /// Return the hash value of the Key.
    size_t hash() const {
        static std::hash<Float> float_hash;
        switch (m_repr_ix) {
            case NIL:   return 0;
            case BOOL:  return (size_t)m_repr.b;
            case INT:   return (size_t)m_repr.i;
            case UINT:  return (size_t)m_repr.u;
            case FLOAT: return (size_t)float_hash(m_repr.f);
            case STR:   return (size_t)m_repr.s.data().data();
            default:    throw wrong_type(m_repr_ix);
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
  friend struct python::Support;
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

} // namespace nodel


namespace std {

//////////////////////////////////////////////////////////////////////////////
/// Key hash function support
//////////////////////////////////////////////////////////////////////////////
template<>
struct hash<nodel::Key>
{
    std::size_t operator () (const nodel::Key& key) const noexcept {
      return key.hash();
    }
};

} // namespace std
