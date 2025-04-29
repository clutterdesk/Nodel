/// @b License: @n Apache License v2.0
/// @copyright Robert Dunnagan
/** @file */
#pragma once

#include <any>
#include <limits>
#include <string>
#include <string_view>
#include <vector>
#include <tuple>
#include <map>
#include <tsl/ordered_map.h>
#include <unordered_set>
#include <stack>
#include <deque>
#include <iostream>
#include <iomanip>
#include <functional>
#include <cstdint>
#include <memory>
#include <type_traits>

#include "Oid.hxx"
#include "Key.hxx"
#include "Slice.hxx"

#include <nodel/support/Flags.hxx>
#include <nodel/support/logging.hxx>
#include <nodel/support/string.hxx>
#include <nodel/support/exception.hxx>
#include <nodel/support/parse.hxx>
#include <nodel/support/types.hxx>


namespace nodel {

struct EmptyReference : public NodelException
{
    EmptyReference() : NodelException("uninitialized object"s) {}
};

struct WriteProtect : public NodelException
{
    WriteProtect() : NodelException("Data-source is write protected"s) {}
};

struct ClobberProtect : public NodelException
{
    ClobberProtect() : NodelException("Data-source is clobber protected"s) {}
};

struct DataSourceError : public NodelException
{
    DataSourceError(std::string&& error) : NodelException(std::forward<std::string>(error)) {}
};

struct InvalidPath : public NodelException
{
    InvalidPath() : NodelException("Invalid object path"s) {}
};

class URI;
class Object;
class Opaque;
class OPath;
class DataSource;
class KeyIterator;
class ValueIterator;
class ItemIterator;
class KeyRange;
class ValueRange;
class ItemRange;
class AncestorRange;

struct NoPredicate {};
using Predicate = std::function<bool(const Object&)>;

using String = std::string;
using ObjectList = std::vector<Object>;
using SortedMap = std::map<Key, Object>;
using OrderedMap = tsl::ordered_map<Key, Object, KeyHash>;

using Item = std::pair<Key, Object>;
using ConstItem = std::pair<const Key, const Object>;
using KeyList = std::vector<Key>;
using ItemList = std::vector<Item>;

// inplace reference count types (ref-count stored in parent bit-field)
using IRCString = std::tuple<String, Object>;
using IRCList = std::tuple<ObjectList, Object>;
using IRCMap = std::tuple<SortedMap, Object>;
using IRCOMap = std::tuple<OrderedMap, Object>;

using IRCStringPtr = IRCString*;
using IRCListPtr = IRCList*;
using IRCMapPtr = IRCMap*;
using IRCOMapPtr = IRCOMap*;
using DataSourcePtr = DataSource*;

using OpaquePtr = Opaque*;

namespace test { class DataSourceTestInterface; }


/////////////////////////////////////////////////////////////////////////////
/// Nodel dynamic object.
/// - Like Python objects, an Object is a reference to its backing data.
///   The assignment operator does not copy the backing data - it copies the
///   reference.
/// - Objects are garbage collected.
/// - Objects have a reference count and are *not* thread-safe.  However
///   an Object can be accessed from different threads synchronously.
/// - The backing data is one of the following types:
///     - nil              (similar to "None" in Python)
///     - boolean
///     - integer          (64-bit, as defined in nodel/support/types.h)
///     - unsigned integer (64-bit, as defined in nodel/support/types.h)
///     - floating point   (64-bit, as defined in nodel/support/types.h)
///     - string           (may represent either text, or binary data)
///     - list             (a recursive list of Objects)
///     - ordered map      (an insertion ordered map, Key -> Object)
///     - sorted map       (a sorted map, Key -> Object)
///     - any              (std::any)
/// - The NIL, BOOL, INT, UINT and FLOAT data types are stored in the Object
///   by-value.  Objects containing these types do not have a reference count,
///   or a parent.
/// - The `get`, `set`, and `del` methods can be used for both lists and maps.
/// - When working with a list, Key instances are integers.  Lists and maps
///   are referred to collectively as "containers".
/// - In general the `get` and `set` methods are faster than the subscript
///   operator.  However a chain of subscript operators will result in a
///   single call to `get` or `set` with an `OPath` instance, which can be
///   optimized by a DataSource implementation.
/// - The `get`, `set`, and `del` methods do not accept `const char*` strings,
///   since this would be the most common usage, and (to my knowledge) can't be
///   optimized since it's not possible to know whether a `const char*` is a
///   read-only literal.
/// - Compiling with `NODEL_ARCH=32` on 32-bit architectures will reduce the
///   size of Object instances from 16-bytes to 8-bytes.
/////////////////////////////////////////////////////////////////////////////

class Object
{
  public:
    template <class T> class Subscript;
    template <class ValueType, typename VisitPred, typename EnterPred> class TreeRange;

    // Enumeration representing the type of the backing data.
    enum ReprIX {
        EMPTY,   // uninitialized reference
        NIL,     // json null, and used to indicate non-existence
        BOOL,
        INT,
        UINT,
        FLOAT,
        STR,     // text or binary data
        LIST,
        SMAP,    // sorted map
        OMAP,    // ordered map
        ANY,     // any object
        DSRC,    // DataSource
        DEL,     // indicates deleted key in sparse data-store
        ERROR = 31
    };

  protected:
      union Repr {
          Repr()                : z{nullptr} {}
          Repr(bool v)          : b{v} {}
          Repr(Int v)           : i{v} {}
          Repr(UInt v)          : u{v} {}
          Repr(Float v)         : f{v} {}
          Repr(IRCStringPtr p)  : ps{p} {}
          Repr(IRCListPtr p)    : pl{p} {}
          Repr(IRCMapPtr p)     : psm{p} {}
          Repr(IRCOMapPtr p)    : pom{p} {}
          Repr(OpaquePtr p)     : pany{p} {}
          Repr(DataSourcePtr p) : ds{p} {}

          void*         z;
          bool          b;
          Int           i;
          UInt          u;
          Float         f;
          IRCStringPtr  ps;
          IRCListPtr    pl;
          IRCMapPtr     psm;
          IRCOMapPtr    pom;
          OpaquePtr     pany;
          DataSourcePtr ds;
      };

      struct Fields
      {
          Fields(uint8_t repr_ix)      : repr_ix{repr_ix}, ref_count{1} {}
          Fields(const Fields& fields) : repr_ix{fields.repr_ix}, ref_count{1} {}
          Fields(Fields&&) = delete;
          auto operator = (const Fields&) = delete;
          auto operator = (Fields&&) = delete;

#if NODEL_ARCH == 32
          uint8_t repr_ix:5;
          uint32_t ref_count:27;
#else
          uint8_t repr_ix:8;
          uint64_t ref_count: 56;
#endif
      };

  private:
    template <typename T> ReprIX get_repr_ix() const {
        if constexpr (std::is_same<T, bool>::value)            return BOOL;
        else if constexpr (std::is_same<T, Int>::value)        return INT;
        else if constexpr (std::is_same<T, UInt>::value)       return UINT;
        else if constexpr (std::is_same<T, Float>::value)      return FLOAT;
        else if constexpr (std::is_same<T, String>::value)     return STR;
        else if constexpr (std::is_same<T, ObjectList>::value) return LIST;
        else if constexpr (std::is_same<T, SortedMap>::value)  return SMAP;
        else if constexpr (std::is_same<T, OrderedMap>::value) return OMAP;
        else if constexpr (std::is_base_of<Opaque, T>::value)  return ANY;
    }

    template <typename T> T get_repr() const requires is_byvalue<T> {
        if constexpr (std::is_same<T, bool>::value)            { return m_repr.b; }
        else if constexpr (std::is_same<T, Int>::value)        { return m_repr.i; }
        else if constexpr (std::is_same<T, UInt>::value)       { return m_repr.u; }
        else if constexpr (std::is_same<T, Float>::value)      { return m_repr.f; }
    }

    template <typename T> T& get_repr() requires is_byvalue<T> {
        if constexpr (std::is_same<T, bool>::value)            { return m_repr.b; }
        else if constexpr (std::is_same<T, Int>::value)        { return m_repr.i; }
        else if constexpr (std::is_same<T, UInt>::value)       { return m_repr.u; }
        else if constexpr (std::is_same<T, Float>::value)      { return m_repr.f; }
    }

    template <typename T> const T& get_repr() const requires std::is_same<T, String>::value { return std::get<0>(*m_repr.ps); }
    template <typename T> T& get_repr() requires std::is_same<T, String>::value             { return std::get<0>(*m_repr.ps); }

    template <typename T> T get_repr() const requires std::is_same<T, ObjectList>::value;

  private:
    struct NoParent {};
    Object(NoParent&&) : m_repr{}, m_fields{NIL} {}  // initialize reference count

  public:
    /// Returns a text description of the type enumeration.
    /// @param repr_ix The value of the ReprIX enumeration.
    static std::string_view type_name(uint8_t repr_ix) {
      switch (repr_ix) {
          case EMPTY: return "empty";
          case NIL:   return "nil";
          case BOOL:  return "bool";
          case INT:   return "int";
          case UINT:  return "uint";
          case FLOAT: return "double";
          case STR:   return "string";
          case LIST:  return "list";
          case SMAP:  return "sorted-map";
          case OMAP:  return "ordered-map";
          case DSRC:  return "data-source";
          case ANY:   return "opaque";
          case DEL:   return "deleted";
          case ERROR: return "error";
          default:    return "<undefined>";
      }
    }

  public:
    /// Indicates an object with a data type that is not reference counted.
    /// @see See `Object::ref_count` method.
    static constexpr refcnt_t no_ref_count = std::numeric_limits<refcnt_t>::max();

    /// Create an *empty* Object.
    /// *Empty* objects behave like references that don't point to anything.  Any
    /// attempt to access data will result in an EmptyReference exception being
    /// thrown.
    Object()                           : m_repr{}, m_fields{EMPTY} {}

    /// Create a reference to nil
    Object(nil_t)                      : m_repr{}, m_fields{NIL} {}
    /// Copy string
    Object(const String& str)          : m_repr{new IRCString{str, NoParent()}}, m_fields{STR} {}
    /// Move string
    Object(String&& str)               : m_repr{new IRCString(std::forward<String>(str), NoParent())}, m_fields{STR} {}
    /// Copy string view
    Object(const StringView& sv)       : m_repr{new IRCString{{sv.data(), sv.size()}, NoParent()}}, m_fields{STR} {}
    /// Copy plain string
    Object(const char* v)              : m_repr{new IRCString{v, NoParent()}}, m_fields{STR} { ASSERT(v != nullptr); }
    /// From boolean value
    Object(bool v)                     : m_repr{v}, m_fields{BOOL} {}
    /// From float value
    Object(is_like_Float auto v)       : m_repr{(Float)v}, m_fields{FLOAT} {}
    /// From signed integer value
    Object(is_like_Int auto v)         : m_repr{(Int)v}, m_fields{INT} {}
    /// From unsigned integer value
    Object(is_like_UInt auto v)        : m_repr{(UInt)v}, m_fields{UINT} {}
    /// Opaque object
    Object(std::unique_ptr<Opaque>& p_opaque)  : m_repr{p_opaque.release()}, m_fields{ANY} {}
    Object(std::unique_ptr<Opaque>&& p_opaque) : m_repr{p_opaque.release()}, m_fields{ANY} {}

    /// Create an Object with a DataSource (Use @ref nodel::bind, instead)
    /// - This is a low-level interface. Prefer using one of the @ref nodel::bind
    ///   functions, so that you can take advantage of configuring the DataSource
    ///   from a URI.
    Object(DataSourcePtr ptr);

    /// Copy Objects from container
    Object(const ObjectList&);
    /// Copy Key/Object pairs from container
    Object(const SortedMap&);
    /// Copy Key/Object pairs from container
    Object(const OrderedMap&);
    /// Move Key/Object pairs from container
    Object(ObjectList&&);
    /// Move Key/Object pairs from container
    Object(SortedMap&&);
    /// Move Key/Object pairs from container
    Object(OrderedMap&&);

    // Create Object from Key
    Object(const Key&);
    // Create Object from Key
    Object(Key&&);

    /// Construct with a new, default value for the specified data type.
    /// @param type The data type.
    Object(ReprIX type);

    /// Implicit conversion of Subscript<Key>
    Object(const Subscript<Key>& sub);
    /// Implicit conversion of Subscript<OPath>
    Object(const Subscript<OPath>& sub);

    Object(const Object& other);
    Object(Object&& other);

    ~Object() { dec_ref_count(); }

    ReprIX type() const;

    /// Returns a readable name for the type of the Object
    std::string_view type_name() const { return type_name(type()); }

    Object root() const;
    Object parent() const;

    KeyRange iter_keys() const;
    ItemRange iter_items() const;
    ValueRange iter_values() const;

    KeyRange iter_keys(const Slice&) const;
    ItemRange iter_items(const Slice&) const;
    ValueRange iter_values(const Slice&) const;

    AncestorRange iter_ancestor() const;

    TreeRange<Object, NoPredicate, NoPredicate> iter_tree() const;

    template <typename VisitPred>
    TreeRange<Object, Predicate, NoPredicate> iter_tree(VisitPred&&) const;

    template <typename EnterPred>
    TreeRange<Object, NoPredicate, Predicate> iter_tree_if(EnterPred&&) const;

    template <typename VisitPred, typename EnterPred>
    TreeRange<Object, Predicate, Predicate> iter_tree_if(VisitPred&&, EnterPred&&) const;

    size_t size() const;

    const Key key() const;
    const Key key_of(const Object&) const;
    OPath path() const;
    OPath path(const Object& root) const;

    bool is_nil() const { return m_fields.repr_ix == NIL; }

    template <typename T>
    bool is_type() const;

    template <typename V>
    void visit(V&& visitor) const;

    template <typename T>
    T as() const requires is_byvalue<T>;

    template <typename T>
    const T& as() const requires std::is_same<T, String>::value;

    template <typename T>
    T& as() requires is_byvalue<T>;

    template <typename T>
    T& as() requires std::is_same<T, String>::value;

//    template <typename T>
//    T as() const requires std::is_same<T, ObjectList>::value;

    template <typename T>
    T& as();

    template <typename T>
    const T& as() const;

    bool is_empty() const;
    bool is_deleted() const;
    bool is_valid() const;

    template <typename T>
    T cast() const;

    Key to_key() const;
    String to_str() const;
    String to_json() const;
    void to_json(std::ostream&) const;

    Object get(is_like_Int auto) const;
    Object get(const Key&) const;
    Object get(const OPath&) const;
    ObjectList get(const Slice&) const;

    Object set(const Object&);
    Object set(const Key&, const Object&);
    Object set(const OPath&, const Object&);
    void set(const Slice&, const Object&);
    void set(const Slice&, ValueRange&&);
    Object insert(const Key&, const Object&);

    void del(const Key&);
    void del(const OPath&);
    void del(const Slice&);
    void del_from_parent();
    void clear();

    Subscript<Key> operator [] (const Key& key);
    Subscript<OPath> operator [] (const OPath& path);

    bool operator == (const Object&) const;
    bool operator == (nil_t) const;
    std::partial_ordering operator <=> (const Object&) const;

    Oid id() const;
    bool is(const Object& other) const;
    Object copy() const;
    refcnt_t ref_count() const;
    void inc_ref_count() const;
    void dec_ref_count() const;
    void release();
    void refer_to(const Object&);

    Object& operator = (const Object& other);
    Object& operator = (Object&& other);

    template <class DataSourceType>
    DataSourceType* data_source() const;

    void needs_saving();
    void save();
    void reset();
    void reset_key(const Key&);
    void refresh();
    void refresh_key(const Key&);

    static WrongType wrong_type(uint8_t actual)                   { return type_name(actual); };
    static WrongType wrong_type(uint8_t actual, uint8_t expected) { return {type_name(actual), type_name(expected)}; };
    static EmptyReference empty_reference()                       { return {}; }

  protected:
    static bool norm_index(is_like_Int auto& index, UInt size);
    Object list_set(const Key& key, const Object& in_val);

    Object dsrc_read() const;

    void set_parent(const Object& parent) const;
    void clear_parent() const;
    void weak_refer_to(const Object&);

  protected:
    Repr m_repr;
    Fields m_fields;

  friend class DataSource;
  friend class KeyIterator;
  friend class ValueIterator;
  friend class ItemIterator;
  friend class AncestorRange;
  friend class KeyRange;
  friend class ValueRange;
  friend class ItemRange;
  friend struct python::Support;

  friend class WalkDF;
  friend class WalkBF;

  template <class ValueType, typename VisitPred, typename EnterPred> friend class TreeIterator;

  friend bool has_data_source(const Object&);
};


class Opaque
{
  public:
    virtual ~Opaque() = default;
    virtual Opaque* clone() = 0;
    virtual String str() = 0;
    virtual String repr() = 0;

    virtual bool equals(const Object& obj) {
        return obj.type() == Object::ANY && &(obj.as<Opaque>()) == this;
    }

    Object& get_parent() { return m_parent; }

  private:
    Object m_parent{nil};
};


//////////////////////////////////////////////////////////////////////////////
/// A simple path consisting of a list of keys.
/// Path literals can be created using `""_path` literal operator.
/// @see nodel::Object::get(const OPath&)
/// @see nodel::Object::set(const OPath&, const Object&)
/// @see nodel::Query
//////////////////////////////////////////////////////////////////////////////
class OPath
{
  public:
    using Iterator = KeyList::iterator;
    using ConstIterator = KeyList::const_iterator;

    OPath() {}

    /// Construct a path from a string path specification.
    /// @param spec The string to parse.
    /// @see OPath operator ""_path (const char*, size_t);
    OPath(const StringView& spec);

    // helpers for Subscript class
    OPath(const Key& l_key, const Key& r_key);
    OPath(const Key& l_key, const OPath& r_path);
    OPath(const OPath& l_path, const Key& r_key);
    OPath(const OPath& l_path, const OPath& r_path);

    OPath(const KeyList& keys) : m_keys{keys} {}
    OPath(KeyList&& keys)      : m_keys{std::forward<KeyList>(keys)} {}

    void append(const Key& key) { m_keys.push_back(key); }
    void append(Key&& key)      { m_keys.emplace_back(std::forward<Key>(key)); }

    /// Returns a copy of this path excepting the last key.
    /// - If an Object, `x`, would be returned by this path, then the path
    ///   returned by this function would return the `x` Object's parent.
    /// @return Returns the parent path.
    OPath parent() const {
        if (m_keys.size() < 2) return KeyList{nil};
        return KeyList{m_keys.begin(), m_keys.end() - 1};
    }

    Object lookup(const Object& origin) const {
        Object obj = origin;
        for (auto& key : *this) {
            auto child = obj.get(key);
            ASSERT(!child.is_empty());
            if (child == nil) return child;
            obj.refer_to(child);
        }
        return obj;
    }

    /// Returns true if the object lies on this path.
    /// @param obj The object to test.
    /// @return Returns true if the object is reachable from any of its ancestors via
    /// this path.
    bool is_leaf(const Object& obj) const {
        Object cursor = obj;
        Object parent = obj.parent();
        for (size_t i = 0; i < m_keys.size() && parent != nil; ++i) {
            cursor = parent;
            parent = cursor.parent();
        }
        if (cursor == nil) return false;
        return lookup(cursor).is(obj);
    }

    /// Create objects necessary to complete this path.
    /// @param origin The starting point.
    /// @param last_value The value that will become the leaf of the path.
    /// - Intermediate containers that do not exist are created with a container
    ///   type that depends on the data type of the Key. If Key data type is an
    ///   integer, then an ObjectList is created. Otherwise, an OrderedMap is
    ///   created.
    /// - If the `last_value` argument has a parent then the copy that was
    ///   added to the last container will be returned.
    /// @return Returns `last_value` or a copy of `last_value`.
    Object create(const Object& origin, const Object& last_value) const {
        Object obj = origin;
        auto it = begin();
        auto it_end = end();
        if (it != it_end) {
            auto prev_it = it;
            while (++it != it_end) {
                auto child = obj.get(*prev_it);
                if (child == nil) {
                    child = Object{(*it).is_any_int()? Object::LIST: Object::OMAP};
                    obj.set(*prev_it, child);
                }
                obj.refer_to(child);
                prev_it = it;
            }
            return obj.set(*prev_it, last_value);
        } else {
            return last_value;
        }
    }

    String to_str() const {
        if (m_keys.size() == 0) return ".";
        StringStream ss;
        auto it = begin();
        if (it != end()) {
            it->to_step(ss, true);
            ++it;
        }
        for (; it != end(); ++it)
            it->to_step(ss);
        return ss.str();
    }

    size_t hash() const {
        size_t result = 0;
        for (auto& key : *this)
            result ^= key.hash();
        return result;
    }

    void reverse()       { std::reverse(m_keys.begin(), m_keys.end()); }
    KeyList& keys()      { return m_keys; }
    KeyList keys() const { return m_keys; }
    Key leaf()           { return m_keys[m_keys.size() - 1]; }

    Iterator begin() { return m_keys.begin(); }
    Iterator end() { return m_keys.end(); }
    ConstIterator begin() const { return m_keys.cbegin(); }
    ConstIterator end() const { return m_keys.cend(); }

  private:
    StringView parse_quoted(const StringView&, StringView::const_iterator&);
    void consume_whitespace(const StringView&, StringView::const_iterator&);
    Key parse_brace_key(const StringView&, StringView::const_iterator&);
    Key parse_dot_key(const StringView&, StringView::const_iterator&);

  private:
    KeyList m_keys;

  friend class Object;
};


inline
OPath::OPath(const Key& l_key, const Key& r_key) {
    m_keys.reserve(2);
    append(l_key);
    append(r_key);
}

inline
OPath::OPath(const Key& l_key, const OPath& r_path) {
    m_keys.reserve(r_path.m_keys.size() + 1);
    append(l_key);
    for (auto& r_key : r_path)
        append(r_key);
}

inline
OPath::OPath(const OPath& l_path, const Key& r_key) {
    m_keys = l_path.m_keys;
    append(r_key);
}

inline
OPath::OPath(const OPath& l_path, const OPath& r_path) {
    m_keys.reserve(l_path.m_keys.size() + r_path.m_keys.size());
    for (auto& l_key : l_path)
        append(l_key);
    for (auto& r_key : r_path)
        append(r_key);
}

inline
OPath::OPath(const StringView& spec) {
    assert (spec.size() > 0);
    auto it = spec.cbegin();
    consume_whitespace(spec, it);
    char c = *it;
    if (c != '.' && c != '[') {
        append(parse_dot_key(spec, it));
    }

    while (it != spec.cend()) {
        char c = *it;
        if (c == '.') {
            if (++it != spec.cend()) {
                append(parse_dot_key(spec, it));
            }
        } else if (c == '[') {
            if (++it != spec.cend()) {
                append(parse_brace_key(spec, it));
            }
        } else {
            throw parse::SyntaxError{spec, it - spec.cbegin(), "Expected '.' or '[':"};
        }
    }
}

inline
Key OPath::parse_brace_key(const StringView& spec, StringView::const_iterator& it) {
    auto key_start = it;
    char c = *it;
    if (c == '\'' || c == '"') {
        auto key = parse_quoted(spec, it);
        if (key.size() == 0) throw parse::SyntaxError{spec, key_start - spec.cbegin(), "Expected key:"};
        consume_whitespace(spec, it);
        if (*it != ']') throw parse::SyntaxError{spec, key_start - spec.cbegin(), "Missing closing ']':"};
        ++it;
        return key;
    } else {
        for (; it != spec.cend() && c != ']'; ++it, c = *it);
        if (it == spec.cend()) throw parse::SyntaxError{spec, key_start - spec.cbegin(), "Missing closing ']':"};
        StringView key{key_start, it};
        if (it != spec.cend()) ++it;
        return str_to_int(key);  // TODO: error handling
    }
}

inline
Key OPath::parse_dot_key(const StringView& spec, StringView::const_iterator& it) {
    auto key_start = it;
    for (char c = *it; it != spec.cend() && (std::isalnum(c) || c == '_'); ++it, c = *it);
    return StringView{key_start, it};
}

inline
StringView OPath::parse_quoted(const StringView& spec, StringView::const_iterator& it) {
    consume_whitespace(spec, it);
    if (it == spec.cend()) return "";
    char quote = *it; ++it;
    auto start_it = it;
    bool escaped = false;
    for (; it != spec.cend(); ++it) {
        if (escaped) {
            escaped = false;
            continue;
        }
        char c = *it;
        if (c == '\\') {
            escaped = true;
        } else if (c == quote) {
            return {start_it, it++};
        }
    }
    throw parse::SyntaxError(spec, spec.size() - 1, "Missing closing quote:");
}

inline
void OPath::consume_whitespace(const StringView& spec, StringView::const_iterator& it) {
    for (char c = *it; std::isspace(c) && it != spec.cend(); ++it);
}

inline
OPath operator ""_path (const char* str, size_t size) {
    return OPath{StringView{str, size}};
}

/// Get the path from the root to this Object
/// - Get an @ref OPath instance that would yield this Object from the
///   following expression: `this->root().get(path)`
/// - If the Object does not have a parent then an empty path is returned.
/// @return Returns a (possibly empty) OPath instance.
/// @see Object::get(const OPath&)
/// @see Object::set(const OPath&, const Object&)
inline
OPath Object::path() const {
    OPath path;
    Object obj = *this;
    Object par = parent();
    while (par != nil) {
        path.append(par.key_of(obj));
        obj.refer_to(par);
        par.refer_to(obj.parent());
    }
    path.reverse();
    return path;
}

/// Get the path from the specified ancestor to this Object
/// @param root An ancestor of this Object.
/// - This method is similar to the Object::path() method except that
///   you can specify an ancestor other than the root of the tree.
/// - If `root` is not an ancestor of this Object then it is ignored
///   and the root of the tree is used, instead.
/// @return Returns a (possibly empty) OPath instance.
/// @see Object::get(const OPath&)
/// @see Object::set(const OPath&, const Object&)
inline
OPath Object::path(const Object& root) const {
    if (root == nil) return path();
    OPath path;
    Object obj = *this;
    Object par = parent();
    while (par != nil && !obj.is(root)) {
        path.append(par.key_of(obj));
        obj.refer_to(par);
        par.refer_to(obj.parent());
    }
    path.reverse();
    return path;
}


//////////////////////////////////////////////////////////////////////////////
/// Base class for objects implementing external data access.
/// - The backing data of an Object may be a subclass of DataSource. This is
///   how on-demand/lazy loading is implemented. The DataSource subclass
///   overrides virtual methods that read/write data from/to the external
///   storage location.
/// - End-users *should not* use the DataSource interface directly.
/// - There are two types of DataSources: *complete* and *sparse*.
///   - A *complete* DataSource loads *all* of its data when it's owning Object
///     is first accessed.
///   - A *sparse* DataSource must be a map. It loads each key independently
///     when the key is first accessed. Sparse DataSources are designed for
///     large databases, and *must* provide iterators.
/// - The iterators provided by *sparse* DataSource implementations are used
///   under-the-covers by all Object methods that return iterators, and
///   provide a seamless, memory-efficient, means of traversing the data.
/// - Ultimately, it's up to the end-user of the Object class to insure that
///   data is accessed efficiently. You should not, for example, call the
///   `Object::keys()` method, which returns a KeyList, if you might be working
///   with a very large domain of keys. Prefer the Object iteration methods,
///   instead.
/// - The first argument of each of the virtual methods is the Object that owns
///   the DataSource instance. This provides context to the DataSource
///   implementation. In particular, the implementation can use the path of
///   the Object.
/// - A DataSource implementation can be hierarchical and populate a container
///   with Objects that, themselves, have DataSources. This is how the
///   filesystem DataSources work.
//////////////////////////////////////////////////////////////////////////////
class DataSource
{
  public:
    class KeyIterator
    {
      public:
        KeyIterator() : m_key{nil} {}
        virtual ~KeyIterator() {}
        void next() { if (!next_impl()) m_key = nil; }
        Key& key() { return m_key; }
        bool done() const { return m_key == nil; }

      protected:
        virtual bool next_impl() = 0;
        Key m_key;
    };

    class ValueIterator
    {
      public:
        ValueIterator() : m_value{nil} {}
        virtual ~ValueIterator() {}
        void next() { if (!next_impl()) m_value.release(); }
        Object& value() { return m_value; }
        bool done() const { return m_value.is_empty(); }

      protected:
        virtual bool next_impl() = 0;
        Object m_value;
    };

    class ItemIterator
    {
      public:
        ItemIterator() : m_item{nil, nil} {}
        virtual ~ItemIterator() {}
        void next() { if (!next_impl()) std::get<0>(m_item) = nil; }
        Item& item() { return m_item; }
        bool done() const { return m_item.first == nil; }

      protected:
        virtual bool next_impl() = 0;
        Item m_item;
    };

  public:
    using ReprIX = Object::ReprIX;

    enum class Kind { SPARSE, COMPLETE };  // SPARSE -> big key/value source, COMPLETE -> all keys present once cached
    enum class Origin { SOURCE, MEMORY };  // SOURCE -> created while reading source, MEMORY -> created by program
    enum class Multilevel { NO, YES };

    struct Mode : public Flags<uint8_t> {
        FLAG8 READ = 1;
        FLAG8 WRITE = 2;
        FLAG8 CLOBBER = 4;
        FLAG8 ALL = 7;
        FLAG8 INHERIT = 8;
        Mode(Flags<uint8_t> flags) : Flags<uint8_t>(flags) {}
        operator int () const { return m_value; }
    };

    /// Options common to all DataSource implementations.
    /// Options::mode can be set from a URI with the "perm" key in the query.
    /// Valid "perm" values are one of the following case-insensitive letters:
    /// - R - read
    /// - W - write
    /// - C - clobber (overwrite)
    /// For example: file://?path=.&perm=rw
    struct Options {
        Options() = default;
        Options(Mode mode) : mode{mode} {}

        /// Configure options from the specified URI query.
        void configure(const Object& uri);

        /// Access control for get/set/del methods
        /// - CLOBBER access controls whether a bound Object may be completely overwritten with
        ///   a single call to Object::set(const Object&).
        Mode mode = Mode::READ | Mode::WRITE;

        /// Logging control during read operations
        bool quiet_read = false;

        /// Logging control during write operations
        bool quiet_write = false;

        /// Throw exception when read operation fails
        bool throw_read_error = true;

        /// Throw exception when write operation fails
        bool throw_write_error = true;
    };

    // TODO: comment
    DataSource(Kind kind, Origin origin, Multilevel multi_level=Multilevel::NO)
      : m_parent{Object::NIL}
      , m_cache{Object::EMPTY}
      , m_kind{kind}
      , m_repr_ix{Object::EMPTY}
      , m_multi_level{multi_level}
      , m_fully_cached{(kind == Kind::COMPLETE) && origin == Origin::MEMORY}
      , m_unsaved{origin == Origin::MEMORY}
    {}

    DataSource(Kind kind, ReprIX repr_ix, Origin origin, Multilevel multi_level=Multilevel::NO)
      : m_parent{Object::NIL}
      , m_cache{repr_ix}
      , m_kind{kind}
      , m_repr_ix{repr_ix}
      , m_multi_level{multi_level}
      , m_fully_cached{(kind == Kind::COMPLETE) && origin == Origin::MEMORY}
      , m_unsaved{origin == Origin::MEMORY}
    {}

    virtual ~DataSource() {}

  protected:
    /// Create a new instance of this DataSource.
    /// @param target The target that will receive this DataSource
    /// @param origin Whether data originates in memory, or from external storage
    /// @return Returns a heap-allocated DataSource instance.
    virtual DataSource* new_instance(const Object& target, Origin origin) const = 0;

    /// Configure this DataSource from a URI.
    /// @param uri A map containing the parts of the URI.
    /// @see nodel/core/uri.h.
    virtual void configure(const Object& uri) { m_options.configure(uri); }

    /// Determine the type of data in external storage.
    /// @param target The target holding this DataSource.
    /// - Implementations should override this method when the type of data in
    ///   storage is dynamic.
    /// - Implementations should use metadata where possible to make this
    ///   operation inexpensive. This method may be called in situations where
    ///   the end-user does not want to incur the overhead of loading the data.
    virtual void read_type(const Object& target) {}

    /// Read all data from external storage.
    /// @param target The target holding this DataSource.
    /// - This method must be implemented by both *complete* and *sparse*
    ///   DataSources, although end-users can easily avoided this flow with
    ///   *sparse* DataSources.
    /// - Implementations call the `read_set` methods to populate the data.
    virtual void read(const Object& target) = 0;

    /// Read the data for the specified key from external storage.
    /// @param target The target holding this DataSource.
    /// @param key The key to read.
    /// - *sparse* implementations must override this method.
    /// - Implementations call the `read_set` methods to populate the data.
    /// @return Returns the value of the key, or nil.
    virtual Object read_key(const Object& target, const Key& key) { return {}; }

    /// Write data to external storage.
    /// @param target The target holding this DataSource.
    /// @param data The data to be written.
    virtual void write(const Object& target, const Object& data) = 0;

    /// Write data for the specified key to external storage.
    /// @param target The target holding this DataSource.
    /// @param key The key to write.
    /// @param value The value of the key.
    /// - *sparse* implementations must override this method.
    virtual void write_key(const Object& target, const Key& key, const Object& value) {}

    /// Delete a key from external storage.
    /// @param target The target holding this DataSource.
    /// @param key The key to delete.
    /// - *sparse* implementations must override thist method.
    virtual void delete_key(const Object& target, const Key& key) {}

    /// Called just before the Object::save() method finishes.
    /// @param target The target holding this DataSource.
    /// @param data The memory representation of this DataSource.
    /// @param del_keys The list of deleted keys.
    /// - This method is called at the end of the Object::save() flow to
    ///   allow the implementation to batch updates.
    /// - This method is only called for *sparse* implementations.
    virtual void commit(const Object& target, const Object& data, const KeyList& del_keys) {}

    // interface to use from virtual read methods
    void read_set(const Object& target, const Object& value);
    void read_set(const Object& target, const Key& key, const Object& value);
    void read_set(const Object& target, Key&& key, const Object& value);
    void read_del(const Object& target, const Key& key);

    virtual std::unique_ptr<KeyIterator> key_iter()     { return nullptr; }
    virtual std::unique_ptr<ValueIterator> value_iter() { return nullptr; }
    virtual std::unique_ptr<ItemIterator> item_iter()   { return nullptr; }

    virtual std::unique_ptr<KeyIterator> key_iter(const Slice&)     { return nullptr; }
    virtual std::unique_ptr<ValueIterator> value_iter(const Slice&) { return nullptr; }
    virtual std::unique_ptr<ItemIterator> item_iter(const Slice&)   { return nullptr; }

    virtual void free_resources() {}

  public:
    const Object& get_cached(const Object& target) const;

    size_t size(const Object& target);
    Object get(const Object& target, const Key& key);

    Options options() const           { return m_options; }
    void set_options(Options options) { m_options = options; }

    Mode mode() const        { return m_options.mode; }
    void set_mode(Mode mode) { m_options.mode = mode; }
    Mode resolve_mode() const;

    bool is_fully_cached() const { return m_fully_cached; }
    bool is_sparse() const       { return m_kind == Kind::SPARSE; }
    bool is_multi_level() const  { return m_multi_level == Multilevel::YES; }

    void report_read_error(std::string&& error);
    void report_write_error(std::string&& error);
    bool is_valid(const Object& target);

    void bind(Object& obj);

  private:
    void update_parent_in_cache(const Object&);
    DataSource* copy(const Object& target, Origin origin) const;
    void set(const Object& target, const Object& in_val);
    Object set(const Object& target, const Key& key, const Object& in_val);
    Object set(const Object& target, Key&& key, const Object& in_val);
    void set(const Object& target, const Slice& key, const Object& obj);
    void set(const Object& target, const Slice& key, ValueRange&&);
    void del(const Object& target, const Key& key);
    void del(const Object& target, const Slice& slice);
    void clear(const Object& target);

    void save(const Object& target);

    void reset();
    void reset_key(const Key& key);
    void refresh();
    void refresh_key(const Key& key);

    String to_str(const Object& target) {
        insure_fully_cached(target);
        return m_cache.to_str();
    }

    String to_json(const Object& target) {
        insure_fully_cached(target);
        return m_cache.to_json();
    }

    const Key key_of(const Object& obj) { return m_cache.key_of(obj); }
    ReprIX type(const Object& target)   { if (m_cache.is_empty()) read_type(target); return m_cache.type(); }
    Oid id(const Object& target)        { if (m_cache.is_empty()) read_type(target); return m_cache.id(); }

    void insure_fully_cached(const Object& target);
    void set_unsaved(bool unsaved) { m_unsaved = unsaved; }

    refcnt_t ref_count() const { return m_parent.m_fields.ref_count; }
    void inc_ref_count()       { m_parent.m_fields.ref_count++; }
    refcnt_t dec_ref_count()   { return --(m_parent.m_fields.ref_count); }

  private:
    Object m_parent;
    Object m_cache;
    Kind m_kind;
    ReprIX m_repr_ix;
    Multilevel m_multi_level = Multilevel::NO;  // true if container values may have DataSources
    bool m_fully_cached = false;
    bool m_unsaved = false;
    bool m_read_failed = false;
    bool m_write_failed = false;
    Options m_options;

  friend class Object;
  friend class KeyRange;
  friend class ValueRange;
  friend class ItemRange;
  friend struct python::Support;
  friend class ::nodel::test::DataSourceTestInterface;
  friend Object bind(const URI&, const Object&);
};


inline
bool has_data_source(const Object& obj) {
    return obj.m_fields.repr_ix == Object::DSRC;
}

inline
bool is_fully_cached(const Object& obj) {
    auto p_ds = obj.data_source<DataSource>();
    return p_ds == nullptr || p_ds->is_fully_cached();
}

inline
bool is_number(const Object& obj) {
    auto typ = obj.type();
    return typ == Object::INT || typ == Object::UINT || typ == Object::FLOAT;
}

inline
bool is_integer(const Object& obj) {
    auto typ = obj.type();
    return typ == Object::INT || typ == Object::UINT;
}

inline
bool is_map(const Object& obj) {
    auto typ = obj.type();
    return typ == Object::SMAP || typ == Object::OMAP;
}

inline
bool is_container(const Object& obj) {
    auto typ = obj.type();
    return typ == Object::LIST || typ == Object::OMAP || typ == Object::SMAP;
}


template <typename T> T Object::get_repr() const requires std::is_same<T, ObjectList>::value {
    ObjectList list;
    if (type() == LIST) {
        for (const auto& val : std::get<0>(*m_repr.pl))
            list.push_back(val);
    }
    return list;
}

inline
Object::Object(const Object& other) : m_fields{other.m_fields} {
    switch (m_fields.repr_ix) {
        case EMPTY: m_repr.z = nullptr; break;
        case NIL:   m_repr.z = nullptr; break;
        case BOOL:  m_repr.b = other.m_repr.b; break;
        case INT:   m_repr.i = other.m_repr.i; break;
        case UINT:  m_repr.u = other.m_repr.u; break;
        case FLOAT: m_repr.f = other.m_repr.f; break;
        case STR:   m_repr.ps = other.m_repr.ps; inc_ref_count(); break;
        case LIST:  m_repr.pl = other.m_repr.pl; inc_ref_count(); break;
        case SMAP:  m_repr.psm = other.m_repr.psm; inc_ref_count(); break;
        case OMAP:  m_repr.pom = other.m_repr.pom; inc_ref_count(); break;
        case ANY:   m_repr.pany = other.m_repr.pany; inc_ref_count(); break;
        case DSRC:  m_repr.ds = other.m_repr.ds; m_repr.ds->inc_ref_count(); break;
        case ERROR: m_repr.ps = other.m_repr.ps; inc_ref_count(); break;
    }
}

inline
Object::Object(Object&& other) : m_fields{other.m_fields} {
    switch (m_fields.repr_ix) {
        case EMPTY: m_repr.z = nullptr; break;
        case NIL:   m_repr.z = nullptr; break;
        case BOOL:  m_repr.b = other.m_repr.b; break;
        case INT:   m_repr.i = other.m_repr.i; break;
        case UINT:  m_repr.u = other.m_repr.u; break;
        case FLOAT: m_repr.f = other.m_repr.f; break;
        case STR:   m_repr.ps = other.m_repr.ps; break;
        case LIST:  m_repr.pl = other.m_repr.pl; break;
        case SMAP:  m_repr.psm = other.m_repr.psm; break;
        case OMAP:  m_repr.pom = other.m_repr.pom; break;
        case ANY:   m_repr.pany = other.m_repr.pany; break;
        case DSRC:  m_repr.ds = other.m_repr.ds; break;
        case ERROR: m_repr.ps = other.m_repr.ps; break;
    }

    other.m_fields.repr_ix = EMPTY;
    other.m_repr.z = nullptr;
}

// DataSource ownership transferred
inline
Object::Object(DataSourcePtr ptr) : m_repr{ptr}, m_fields{DSRC} {
    if (ptr != nullptr) ptr->update_parent_in_cache(*this);
}

inline
Object::Object(const ObjectList& list) : m_repr{new IRCList(ObjectList{}, NoParent{})}, m_fields{LIST} {
    auto& my_list = std::get<0>(*m_repr.pl);
    for (auto value : list) {
        value = value.copy();
        my_list.push_back(value);
        value.set_parent(*this);
    }
}

inline
Object::Object(const SortedMap& map) : m_repr{new IRCMap(SortedMap{}, NoParent{})}, m_fields{SMAP} {
    auto& my_map = std::get<0>(*m_repr.psm);
    for (auto [key, value]: map) {
        value = value.copy();
        my_map.insert_or_assign(key, value);
        value.set_parent(*this);
    }
}

inline
Object::Object(const OrderedMap& map) : m_repr{new IRCOMap(OrderedMap{}, NoParent{})}, m_fields{OMAP} {
    auto& my_map = std::get<0>(*m_repr.pom);
    for (auto [key, value]: map) {
        value = value.copy();
        my_map.insert_or_assign(key, value);
        value.set_parent(*this);
    }
}

inline
Object::Object(ObjectList&& list) : m_repr{new IRCList(std::forward<ObjectList>(list), NoParent{})}, m_fields{LIST} {
    auto& my_list = std::get<0>(*m_repr.pl);
    for (auto& obj : my_list) {
        obj.clear_parent();
        obj.set_parent(*this);
    }
}

inline
Object::Object(SortedMap&& map) : m_repr{new IRCMap(std::forward<SortedMap>(map), NoParent{})}, m_fields{SMAP} {
    auto& my_map = std::get<0>(*m_repr.psm);
    for (auto& [key, obj]: my_map) {
        const_cast<Object&>(obj).clear_parent();
        const_cast<Object&>(obj).set_parent(*this);
    }
}

inline
Object::Object(OrderedMap&& map) : m_repr{new IRCOMap(std::forward<OrderedMap>(map), NoParent{})}, m_fields{OMAP} {
    auto& my_map = std::get<0>(*m_repr.pom);
    for (auto& [key, obj]: my_map) {
        const_cast<Object&>(obj).clear_parent();
        const_cast<Object&>(obj).set_parent(*this);
    }
}

inline
Object::Object(const Key& key) : m_fields{EMPTY} {
    switch (key.type()) {
        case Key::NIL:   m_fields.repr_ix = NIL; m_repr.z = nullptr; break;
        case Key::BOOL:  m_fields.repr_ix = BOOL; m_repr.b = key.as<bool>(); break;
        case Key::INT:   m_fields.repr_ix = INT; m_repr.i = key.as<Int>(); break;
        case Key::UINT:  m_fields.repr_ix = UINT; m_repr.u = key.as<UInt>(); break;
        case Key::FLOAT: m_fields.repr_ix = FLOAT; m_repr.f = key.as<Float>(); break;
        case Key::STR: {
            m_fields.repr_ix = STR;
            m_repr.ps = new IRCString{String{key.as<StringView>().data()}, {}};
            break;
        }
        default: throw std::invalid_argument("key");
    }
}

inline
Object::Object(Key&& key) : m_fields{EMPTY} {
    switch (key.type()) {
        case Key::NIL:  m_fields.repr_ix = NIL; m_repr.z = nullptr; break;
        case Key::BOOL:  m_fields.repr_ix = BOOL; m_repr.b = key.as<bool>(); break;
        case Key::INT:   m_fields.repr_ix = INT; m_repr.i = key.as<Int>(); break;
        case Key::UINT:  m_fields.repr_ix = UINT; m_repr.u = key.as<UInt>(); break;
        case Key::FLOAT: m_fields.repr_ix = FLOAT; m_repr.f = key.as<Float>(); break;
        case Key::STR: {
            m_fields.repr_ix = STR;
            m_repr.ps = new IRCString{String{key.as<StringView>().data()}, {}};
            break;
        }
        default: throw std::invalid_argument("key");
    }
}

inline
Object::Object(ReprIX type) : m_fields{(uint8_t)type} {
    switch (type) {
        case EMPTY: m_repr.z = nullptr; break;
        case NIL:  m_repr.z = nullptr; break;
        case BOOL:  m_repr.b = false; break;
        case INT:   m_repr.i = 0; break;
        case UINT:  m_repr.u = 0; break;
        case FLOAT: m_repr.f = 0.0; break;
        case STR:   m_repr.ps = new IRCString{"", NoParent{}}; break;
        case LIST:  m_repr.pl = new IRCList{ObjectList{}, NoParent{}}; break;
        case SMAP:  m_repr.psm = new IRCMap{SortedMap{}, NoParent{}}; break;
        case OMAP:  m_repr.pom = new IRCOMap{OrderedMap{}, NoParent{}}; break;
        case DEL:   m_repr.z = nullptr; break;
        case ERROR: m_repr.ps = new IRCString{"", NoParent{}}; break;
        default:    throw wrong_type(type);
    }
}

inline
Object::ReprIX Object::type() const {
    switch (m_fields.repr_ix) {
        case DSRC: return m_repr.ds->type(*this);
        default:   return (ReprIX)m_fields.repr_ix;
    }
}

/// Returns the root container
inline
Object Object::root() const {
    Object obj = *this;
    Object par = parent();
    while (par != nil) {
        obj.refer_to(par);
        par.refer_to(par.parent());
    }
    return obj;
}

/// Returns the parent container which holds this Object
inline
Object Object::parent() const {
    switch (m_fields.repr_ix) {
        case EMPTY: throw empty_reference();
        case STR:   return std::get<1>(*m_repr.ps);
        case LIST:  return std::get<1>(*m_repr.pl);
        case SMAP:  return std::get<1>(*m_repr.psm);
        case OMAP:  return std::get<1>(*m_repr.pom);
        case ANY:   return m_repr.pany->get_parent();
        case DSRC:  return m_repr.ds->m_parent;
        default:    return nil;
    }
}

/// Similar to the std::visit function
/// @tparam V The type of the visitor function
/// @param visitor The visitor.
/// - If the @ref Object::is_empty method would return true then calling
///   this method will throw an EmptyReference exception.
template <typename V>
void Object::visit(V&& visitor) const {
    switch (m_fields.repr_ix) {
        case EMPTY: throw empty_reference();
        case NIL:   visitor(nullptr); break;
        case BOOL:  visitor(m_repr.b); break;
        case INT:   visitor(m_repr.i); break;
        case UINT:  visitor(m_repr.u); break;
        case FLOAT: visitor(m_repr.f); break;
        case STR:   visitor(std::get<0>(*m_repr.ps)); break;
        case DSRC:  visitor(*this); break;  // TODO: How to visit Object with DataSource?
        default:    throw wrong_type(m_fields.repr_ix);
    }
}

/// Get the stored data iff the data has the specified numeric type
/// @tparam T One of the supported numeric data types
/// - The supported data types are: bool, Int, UInt, Float
/// @return Returns a copy of the stored data type
/// @throw Throws a WrongType exception if the stored data type does not match
template <typename T>
T Object::as() const requires is_byvalue<T> {
    if (m_fields.repr_ix == get_repr_ix<T>()) return get_repr<T>();
    else if (m_fields.repr_ix == DSRC) return dsrc_read().as<T>();
    throw wrong_type(m_fields.repr_ix);
}

/// Get the stored data iff the data has the specified numeric type
/// @tparam T String
/// - This overload of the method is for String
/// @return Returns a const reference to the stored data type
/// @throw Throws a WrongType exception if the stored data type does not match
template <typename T>
const T& Object::as() const requires std::is_same<T, String>::value {
    if (m_fields.repr_ix == get_repr_ix<T>()) return get_repr<T>();
    else if (m_fields.repr_ix == DSRC) return dsrc_read().as<T>();
    throw wrong_type(m_fields.repr_ix);
}

/// Get the stored data iff the data has the specified numeric type
/// @tparam T One of the supported numeric data types
/// - The supported data types are: bool, Int, UInt, Float
/// @return Returns a reference to the stored data type
/// @throw Throws a WrongType exception if the stored data type does not match
template <typename T>
T& Object::as() requires is_byvalue<T> {
    if (m_fields.repr_ix == get_repr_ix<T>()) return get_repr<T>();
    else if (m_fields.repr_ix == DSRC) return dsrc_read().as<T>();
    throw wrong_type(m_fields.repr_ix);
}

/// Get the stored data iff the data has the specified numeric type
/// @tparam T String
/// - This overload of the method is for String
/// @note If this Object has a DataSource and you modify the string data
///       returned by this method, then you must call @ref Object::needs_saving
///       to set the `unsaved` flag, before call @ref Object::save.
/// @return Returns a mutable reference to the String data
/// @throw Throws a WrongType exception if the stored data type does not match
template <typename T>
T& Object::as() requires std::is_same<T, String>::value {
    if (m_fields.repr_ix == get_repr_ix<T>() || m_fields.repr_ix == ERROR) return get_repr<T>();
    else if (m_fields.repr_ix == DSRC) return dsrc_read().as<T>();
    throw wrong_type(m_fields.repr_ix);
}

template <typename T>
T& Object::as() {
    if (m_fields.repr_ix == ANY) return *dynamic_cast<T*>(m_repr.pany);
    throw wrong_type(m_fields.repr_ix);
}

template <typename T>
const T& Object::as() const {
    if (m_fields.repr_ix == ANY) return *dynamic_cast<T*>(m_repr.pany);
    throw wrong_type(m_fields.repr_ix);
}

inline
Object Object::dsrc_read() const {
    return const_cast<DataSourcePtr>(m_repr.ds)->get_cached(*this);
}

inline
void Object::refer_to(const Object& other) {
    operator = (other);
}

/// For internal use only
inline
void Object::weak_refer_to(const Object& other) {
    // NOTE: This method is private, and only intended for use by the set_parent method.
    //       It's part of a scheme for breaking the parent/child reference cycle.
    m_fields.repr_ix = other.m_fields.repr_ix;
    switch (m_fields.repr_ix) {
        case EMPTY: throw empty_reference();
        case NIL:  m_repr.z = nullptr; break;
//        case BOOL:  m_repr.b = other.m_repr.b; break;
//        case INT:   m_repr.i = other.m_repr.i; break;
//        case UINT:  m_repr.u = other.m_repr.u; break;
//        case FLOAT: m_repr.f = other.m_repr.f; break;
//        case STR:   m_repr.ps = other.m_repr.ps; break;
        case LIST:  m_repr.pl = other.m_repr.pl; break;
        case SMAP:  m_repr.psm = other.m_repr.psm; break;
        case OMAP:  m_repr.pom = other.m_repr.pom; break;
        case DSRC:  m_repr.ds = other.m_repr.ds; break;
        default:      throw wrong_type(m_fields.repr_ix);
    }
}

/// For internal use only
inline
void Object::set_parent(const Object& new_parent) const {
    switch (m_fields.repr_ix) {
        case STR: {
            auto& parent = std::get<1>(*m_repr.ps);
            parent.weak_refer_to(new_parent);
            break;
        }
        case LIST: {
            auto& parent = std::get<1>(*m_repr.pl);
            parent.weak_refer_to(new_parent);
            break;
        }
        case SMAP: {
            auto& parent = std::get<1>(*m_repr.psm);
            parent.weak_refer_to(new_parent);
            break;
        }
        case OMAP: {
            auto& parent = std::get<1>(*m_repr.pom);
            parent.weak_refer_to(new_parent);
            break;
        }
        case ANY: {
            auto& parent = m_repr.pany->get_parent();
            parent.weak_refer_to(new_parent);
            break;
        }
        case DSRC: {
            m_repr.ds->m_parent.weak_refer_to(new_parent);
            break;
        }
        default:
            break;
    }
}

/// For internal use only
[[gnu::always_inline]]
inline
void Object::clear_parent() const {
    switch (m_fields.repr_ix) {
        case STR: {
            auto& parent = std::get<1>(*m_repr.ps);
            parent.m_fields.repr_ix = NIL;
            break;
        }
        case LIST: {
            auto& parent = std::get<1>(*m_repr.pl);
            parent.m_fields.repr_ix = NIL;
            break;
        }
        case SMAP: {
            auto& parent = std::get<1>(*m_repr.psm);
            parent.m_fields.repr_ix = NIL;
            break;
        }
        case OMAP: {
            auto& parent = std::get<1>(*m_repr.pom);
            parent.m_fields.repr_ix = NIL;
            break;
        }
        case ANY: {
            auto& parent = m_repr.pany->get_parent();
            parent.m_fields.repr_ix = NIL;
            break;
        }
        case DSRC: {
            m_repr.ds->m_parent.m_fields.repr_ix = NIL;
            break;
        }
        default:
            break;
    }
}

/// Get the key associated with this Object
/// - If this Object has a parent then this method returns the key by which
///   you can access this Object within its parent.
/// - If this Object does not have a parent, then nil is returned.
/// @return Returns nil or the key.
inline
const Key Object::key() const {
    return parent().key_of(*this);
}

/// Get the key associated with the specified Object
/// @param obj The Object.
/// @return Returns nil or the key.
/// @throws WrongType
/// @throws EmptyReference
inline
const Key Object::key_of(const Object& obj) const {
    switch (m_fields.repr_ix) {
        case EMPTY: throw empty_reference();
        case NIL: break;
        case LIST: {
            const auto& list = std::get<0>(*m_repr.pl);
            UInt index = 0;
            for (const auto& item : list) {
                if (item.is(obj))
                    return index;
                index++;
            }
            break;
        }
        case SMAP: {
            const auto& map = std::get<0>(*m_repr.psm);
            for (auto& [key, value] : map) {
                if (value.is(obj))
                    return key;
            }
            break;
        }
        case OMAP: {
            const auto& map = std::get<0>(*m_repr.pom);
            for (auto& [key, value] : map) {
                if (value.is(obj))
                    return key;
            }
            break;
        }
        case DSRC: {
            return m_repr.ds->key_of(obj);
        }
        default:
            throw wrong_type(m_fields.repr_ix);
    }
    return {};
}

/// Check the type of the stored data
/// @tparam T The type to test.
/// - If this Object has a DataSource then it may be necessary to load some or
///   all of the data to determine the type of Object. For example, since a JSON
///   file may contain different types, it's necessary to load at least enough of
///   the beginning of the JSON data to determine the type.
/// @return Returns true if the stored data has the specified type
template <typename T>
bool Object::is_type() const {
    return type() == get_repr_ix<T>();
}

/// Create a string representation of the stored data type
/// - NIL is returned as, "nil"
/// - BOOL is returned as, "true" or "false"
/// - INT, UINT and FLOAT are converted to strings with maximum precision
/// - Containers are converted to JSON strings
/// - A DataSource is converted based on the data type it loads.
/// @throws WrongType
/// @throws EmptyReference
inline
String Object::to_str() const {
    switch (m_fields.repr_ix) {
        case EMPTY: throw empty_reference();
        case NIL:   return "nil";
        case BOOL:  return m_repr.b? "true": "false";
        case INT:   return int_to_str(m_repr.i);
        case UINT:  return int_to_str(m_repr.u);
        case FLOAT: return float_to_str(m_repr.f);
        case STR:   return std::get<0>(*m_repr.ps);
        case LIST:  [[fallthrough]];
        case SMAP:  [[fallthrough]];
        case OMAP:  return to_json();
        case ANY:   return m_repr.pany->str();
        case DSRC:  return const_cast<DataSourcePtr>(m_repr.ds)->to_str(*this);
        case ERROR: return std::get<0>(*m_repr.ps);
        default:    throw wrong_type(m_fields.repr_ix);
    }
}

/// Test if this Object is an empty reference
/// - An Object is an empty reference if it was never assigned a value.
/// - Default constructed Object instances are empty references.
/// - Calling @ref Object::release will result in an empty reference.
/// @return Returns true if the Object is an empty reference.
inline
bool Object::is_empty() const {
    return m_fields.repr_ix == EMPTY;
}

/// Test if this Object has been deleted from a *sparse* DataSource
/// - Sparse DataSource implementations load keys individually, so a special data
///   type is reserved to indicate that a key has been deleted.
/// - This method is only useful for a *sparse* DataSource implementation.
/// @return Returns true if the key associated with this Object was deleted.
inline
bool Object::is_deleted() const {
    return m_fields.repr_ix == DEL;
}

/// Test if this Object was successfully loaded into memory
/// - An Object containing a DataSource configured not to throw exceptions will
///   be marked invalid if a load operation fails.
/// - Use this method to verify that a load operation was successful whenever
///   the DataSource is configured *not* to throw exceptions.
/// @return Returns true if the Object was successfully loaded, or if the
///         Object does not have a DataSource.
inline
bool Object::is_valid() const {
    if (m_fields.repr_ix == ERROR)
        return false;
    else if (m_fields.repr_ix == DSRC)
        return m_repr.ds->is_valid(*this);
    else
        return true;
}

/// Cast the data type stored in this Object to a bool
/// - Casting a container to bool returns true if the size > 0.
/// - Casting an Object with a DataSource is equivalent to casting the loaded value.
/// @throws WrongType
/// @throws EmptyReference
template <> inline
bool Object::cast() const {
    switch (m_fields.repr_ix) {
        case EMPTY: throw empty_reference();
        case NIL:   return false;
        case BOOL:  return m_repr.b;
        case INT:   return m_repr.i;
        case UINT:  return m_repr.u;
        case FLOAT: return m_repr.f;
        case STR:   return str_to_bool(std::get<0>(*m_repr.ps));
        case LIST:  [[fallthrough]];
        case SMAP:  [[fallthrough]];
        case OMAP:  return size() > 0;
        case DSRC:  return const_cast<DataSourcePtr>(m_repr.ds)->get_cached(*this).cast<bool>();
        default:    throw wrong_type(m_fields.repr_ix, BOOL);
    }
}

/// Cast the data type stored in this Object to a signed integer
/// - The following types can be cast to a bool: NIL, BOOL, INT, UINT, FLOAT,
///   STR or DataSources with one of those types.
/// @throws WrongType
/// @throws EmptyReference
template<> inline
Int Object::cast() const {
    switch (m_fields.repr_ix) {
        case EMPTY: throw empty_reference();
        case NIL:   return 0;
        case BOOL:  return m_repr.b;
        case INT:   return m_repr.i;
        case UINT:  return m_repr.u;
        case FLOAT: return m_repr.f;
        case STR:   return str_to_int(std::get<0>(*m_repr.ps));
        case DSRC:  return const_cast<DataSourcePtr>(m_repr.ds)->get_cached(*this).cast<Int>();
        default:    throw wrong_type(m_fields.repr_ix, INT);
    }
}

/// Cast the data type stored in this Object to a signed integer
/// - The following types can be cast to a bool: NIL, BOOL, INT, UINT, FLOAT,
///   STR or DataSources with one of those types.
/// @throws WrongType
/// @throws EmptyReference
template <> inline
UInt Object::cast() const {
    switch (m_fields.repr_ix) {
        case EMPTY: throw empty_reference();
        case NIL:   return 0UL;
        case BOOL:  return m_repr.b;
        case INT:   return m_repr.i;
        case UINT:  return m_repr.u;
        case STR:   return str_to_int(std::get<0>(*m_repr.ps));
        case DSRC:  return const_cast<DataSourcePtr>(m_repr.ds)->get_cached(*this).cast<UInt>();
        default:    throw wrong_type(m_fields.repr_ix, UINT);
    }
}

/// Cast the data type stored in this Object to a signed integer
/// - The following types can be cast to a bool: NIL, BOOL, INT, UINT, FLOAT,
///   STR or DataSources with one of those types.
/// @throws WrongType
/// @throws EmptyReference
template <> inline
Float Object::cast() const {
    switch (m_fields.repr_ix) {
        case EMPTY: throw empty_reference();
        case NIL:   return 0.0;
        case BOOL:  return m_repr.b;
        case INT:   return m_repr.i;
        case UINT:  return m_repr.u;
        case FLOAT: return m_repr.f;
        case STR:   return str_to_float(std::get<0>(*m_repr.ps));
        case DSRC:  return const_cast<DataSourcePtr>(m_repr.ds)->get_cached(*this).cast<Float>();
        default:    throw wrong_type(m_fields.repr_ix, FLOAT);
    }
}

/// Creates a Key with the same value as this Object, if possible
/// - The following types can be cast to a bool: NIL, BOOL, INT, UINT, FLOAT,
///   STR or DataSources with one of those types.
/// @throws WrongType
/// @throws EmptyReference
inline
Key Object::to_key() const {
    switch (m_fields.repr_ix) {
        case EMPTY: throw empty_reference();
        case NIL:   return nil;
        case BOOL:  return m_repr.b;
        case INT:   return m_repr.i;
        case UINT:  return m_repr.u;
        case FLOAT: return m_repr.f;
        case STR:   return std::get<0>(*m_repr.ps);
        case DSRC:  return const_cast<DataSourcePtr>(m_repr.ds)->get_cached(*this).to_key();
        default:    throw wrong_type(m_fields.repr_ix);
    }
}

inline
bool Object::norm_index(is_like_Int auto& index, UInt size) {
    if (index < 0) index += size;
    if (index < 0 || (UInt)index >= size) return false;
    return true;
}

/// Get the value of a key from a container or string
/// @param index An integer key.
/// - This method may provide slightly faster access to list elements by
///   avoiding the creation of a temporary Key.
/// - If the key is less than zero, the size of the container is first
///   added to the key.
/// @throws WrongType
/// @throws EmptyReference
inline
Object Object::get(is_like_Int auto index) const {
    switch (m_fields.repr_ix) {
        case EMPTY: throw empty_reference();
        case LIST: {
            auto& list = std::get<0>(*m_repr.pl);
            if (!norm_index(index, list.size())) return nil;
            return list[index];
        }
        case DSRC:  return m_repr.ds->get(*this, index);
        default:    return get(Key{index});
    }
}

/// Get the value of a key from a container or string
/// @param key The key to lookup.
/// - If the key is less than zero, the size of the container is first
///   added to the key.
/// - If the stored data type is a list then the key must be convertible to
///   a signed integer. The key is used to index the list.
/// - If the stored data type is a map then any key type may be used.
/// - If the stored data type is NIL then NIL is returned.
/// @throws EmptyReference
inline
Object Object::get(const Key& key) const {
    switch (m_fields.repr_ix) {
        case EMPTY: throw empty_reference();
        case LIST: {
            auto& list = std::get<0>(*m_repr.pl);
            if (!nodel::is_integer(key)) throw Key::wrong_type(key.type());
            Int index = key.to_int();
            if (!norm_index(index, list.size())) return nil;
            return list[index];
        }
        case SMAP: {
            auto& map = std::get<0>(*m_repr.psm);
            auto it = map.find(key);
            if (it == map.end()) return nil;
            return it->second;
        }
        case OMAP: {
            auto& map = std::get<0>(*m_repr.pom);
            auto it = map.find(key);
            if (it == map.end()) return nil;
            return it->second;
        }
        case DSRC: return m_repr.ds->get(*this, key); break;
        default:   return nil;
    }
}

/// This method is equivalent to calling @ref OPath::lookup(const Object&).
/// @param path The path whose leaf value will be returned.
/// @throws WrongType
/// @throws EmptyReference
inline
Object Object::get(const OPath& path) const {
    return path.lookup(*this);
}

/// Overwrite (clobber) the stored data type of this Object
/// @param value The value to be assigned.
/// - If this Object has a parent then this method is equivalent to
///   `this->parent().set(this->key(), value)` - that is, it replaces this
///   Object in the parent container.
/// - Otherwise, this method is equivalent to calling the assignment operator.
inline
Object Object::set(const Object& value) {
    auto repr_ix = m_fields.repr_ix;
    if (repr_ix == EMPTY) {
        this->operator = (value);
        return value;
    } else {
        auto par = parent();
        if (par == nil) {
            if (repr_ix == DSRC) {
                m_repr.ds->set(*this, value);
                return value;
            } else {
                this->operator = (value);
                return value;
            }
        } else {
            return par.set(par.key_of(*this), value);
        }
    }
}

inline
ObjectList::size_type norm_index_key(const Key& key, Int list_size) {
    if (!nodel::is_integer(key)) throw Key::wrong_type(key.type());
    Int index = key.to_int();
    if (index < 0) index += list_size;
    if (index < 0 || index > list_size)
        throw std::out_of_range("key");
    return index;
}

inline
Object Object::list_set(const Key& key, const Object& in_val) {
    Object out_val = (in_val.parent() == nil)? in_val: in_val.copy();
    auto& list = std::get<0>(*m_repr.pl);
    auto size = list.size();
    auto index = norm_index_key(key, list.size());
    if (index == size) {
        list.push_back(out_val);
    } else {
        auto it = list.begin() + index;
        it->set_parent(nil);
        *it = out_val;
    }
    out_val.set_parent(*this);
    return out_val;
}

/// Set the value of a key in this container.
/// @param key The key to be assigned.
/// @param in_val The value to be assigned to the key.
/// - If @p in_val already has a parent then it a copy is created and the
///   copy is assigned to the key. An Object can only have one parent.
///   Call @ref Object::del_from_parent() to avoid the copy.
/// - If this Object is a list and the key is negative, the size of the
///   list is added to the key. The list elements are consecutively replaced.
/// @see Object::insert(const Key&, const Object&)
/// @return Returns the Object that was actually inserted, which may be a copy.
/// @throws WrongType
/// @throws EmptyReference
inline
Object Object::set(const Key& key, const Object& in_val) {
    switch (m_fields.repr_ix) {
        case EMPTY: throw empty_reference();
        case LIST: {
            return list_set(key, in_val);
        }
        case SMAP: {
            Object out_val = (in_val.parent() == nil)? in_val: in_val.copy();
            auto& map = std::get<0>(*m_repr.psm);
            auto it = map.find(key);
            if (it != map.end()) {
                it->second.set_parent(nil);
                map.insert_or_assign(it, key, out_val);
            } else {
                map.insert_or_assign(key, out_val);
            }
            out_val.set_parent(*this);
            return out_val;
        }
        case OMAP: {
            Object out_val = (in_val.parent() == nil)? in_val: in_val.copy();
            auto& map = std::get<0>(*m_repr.pom);
            auto it = map.find(key);
            if (it != map.end()) {
                it->second.set_parent(nil);
                map.insert_or_assign(it, key, out_val);
            } else {
                map.insert_or_assign(key, out_val);
            }
            out_val.set_parent(*this);
            return out_val;
        }
        case DSRC: return m_repr.ds->set(*this, key, in_val);
        default:   throw wrong_type(m_fields.repr_ix);
    }
}

/// Equivalent to calling @ref OPath::create(const Object&, const Object&)
/// @note All the same rules apply as for the other @ref Object::set methods.
/// @return Returns the Object that was actually inserted, which may be a copy.
/// @throws WrongType
/// @throws EmptyReference
inline
Object Object::set(const OPath& path, const Object& in_val) {
    return path.create(*this, in_val);
}

/// Insert a value at the specified key in the container.
/// - This method is identical to @ref Object::set(const Key&, const Object&)
///   for maps.
/// - For lists, this method *inserts*, instead of *replaces*, the elements.
/// @return Returns the Object that was actually inserted, which may be a copy.
/// @throws WrongType
/// @throws EmptyReference
inline
Object Object::insert(const Key& key, const Object& in_val) {
    switch (m_fields.repr_ix) {
        case EMPTY: throw empty_reference();
        case LIST: {
            Object out_val = (in_val.parent() == nil)? in_val: in_val.copy();
            auto& list = std::get<0>(*m_repr.pl);
            auto index = norm_index_key(key, list.size());
            list.insert(list.begin() + index, out_val);
            out_val.set_parent(*this);
            return out_val;
        }
        case OMAP: return set(key, in_val);
        case SMAP: return set(key, in_val);
        default:   throw wrong_type(m_fields.repr_ix);
    }
}

/// Delete the value of a key
/// @param key The key to be deleted.
/// - If this Object is a list and the key is negative, the size of the list
///   is added to the key.
/// - If this Object is a string then a WrongType exception is thrown.  Use
///   @ref Object::as<String>(), instead.
/// @throws WrongType
/// @throws EmptyReference
inline
void Object::del(const Key& key) {
    switch (m_fields.repr_ix) {
        case EMPTY: throw empty_reference();
        case LIST: {
            auto& list = std::get<0>(*m_repr.pl);
            Int index = key.to_int();
            auto size = list.size();
            if (index < 0) index += size;
            if (index >= 0) {
                if (index > (Int)size) index = size;
                auto it = list.begin() + index;
                Object value = *it;
                list.erase(it);
                value.clear_parent();
            }
            break;
        }
        case SMAP: {
            auto& map = std::get<0>(*m_repr.psm);
            auto it = map.find(key);
            if (it != map.end()) {
                Object value = it->second;
                value.clear_parent();
                map.erase(it);
            }
            break;
        }
        case OMAP: {
            auto& map = std::get<0>(*m_repr.pom);
            auto it = map.find(key);
            if (it != map.end()) {
                Object value = it->second;
                value.clear_parent();
                map.erase(it);
            }
            break;
        }
        case DSRC: m_repr.ds->del(*this, key); break;
        default:   throw wrong_type(m_fields.repr_ix);
    }
}

/// Delete the leaf of a path
/// @throws WrongType
/// @throws EmptyReference
inline
void Object::del(const OPath& path) {
    auto obj = path.lookup(*this);
    if (obj != nil) {
        auto par = obj.parent();
        par.del(par.key_of(obj));
    }
}

/// Delete this Object from its parent container.
/// @throws WrongType
/// @throws EmptyReference
inline
void Object::del_from_parent() {
    Object par = parent();
    if (par != nil)
        par.del(par.key_of(*this));
}

/// Clear the elements, or string content, of this Object
inline
void Object::clear() {
    switch (m_fields.repr_ix) {
        case EMPTY: throw empty_reference();
        case STR:   std::get<0>(*m_repr.ps).clear(); break;
        case LIST: {
            for (auto& obj : std::get<0>(*m_repr.pl))
                obj.clear_parent();
            std::get<0>(*m_repr.pl).clear();
            break;
        }
        case SMAP: {
            for (auto& item : std::get<0>(*m_repr.psm))
                item.second.clear_parent();
            std::get<0>(*m_repr.psm).clear();
            break;
        }
        case OMAP: {
            for (auto& item : std::get<0>(*m_repr.pom))
                item.second.clear_parent();
            std::get<0>(*m_repr.pom).clear();
            break;
        }
        case DSRC:  m_repr.ds->clear(*this); break;
        default:    throw wrong_type(m_fields.repr_ix);
    }
}

/// Returns the size of the string or container
/// @return Returns the size of the string or container, or 0 for other types.
inline
size_t Object::size() const {
    switch (m_fields.repr_ix) {
        case EMPTY: throw empty_reference();
        case STR:   return std::get<0>(*m_repr.ps).size();
        case LIST:  return std::get<0>(*m_repr.pl).size();
        case SMAP:  return std::get<0>(*m_repr.psm).size();
        case OMAP:  return std::get<0>(*m_repr.pom).size();
        case DSRC:  return m_repr.ds->size(*this);
        default:    return 0;
    }
}


#include "KeyRange.hxx"
#include "ValueRange.hxx"
#include "ItemRange.hxx"

/// Returns a range-like object over container Keys
inline KeyRange Object::iter_keys() const {
    return *this;
}

/// Returns a range-like object over container std::pair<Key, Object>
inline ItemRange Object::iter_items() const {
    return *this;
}

/// Returns a range-like object over container Objects
inline ValueRange Object::iter_values() const {
    return *this;
}

/// Returns a range-like object over a slice of container
inline KeyRange Object::iter_keys(const Slice& slice) const {
    return {*this, slice};
}

/// Returns a range-like object over a slice of container
inline ItemRange Object::iter_items(const Slice& slice) const {
    return {*this, slice};
}

/// Returns a range-like object over a slice of container
inline ValueRange Object::iter_values(const Slice& slice) const {
    return {*this, slice};
}

/// Intended for small intervals - use @ref Object::iter_values(), otherwise
/// @todo Consider whether slicing a map should slice keys or slice the list
///       of map items currently it slices keys, which is useful when a map
///       is used to represent a sparse list
/// @param slice A slice of elements to retrieve.
/// @return Returns a list Object with the (possibly copied) elements.
/// @throws WrongType
/// @throws EmptyReference
inline
ObjectList Object::get(const Slice& slice) const {
    if (m_fields.repr_ix == LIST) {
        auto& list = std::get<0>(*m_repr.pl);
        auto [start, stop, step] = slice.to_indices(list.size());
        return get_slice(list, start, stop, step);
    } else {
        ObjectList result;
        for (auto value : iter_values(slice))
            result.push_back(value);
        return result;
    }
}

/// Set a slice of values.
/// @param slice An interval (slice) of keys.
/// @param in_vals The values to be assigned.
/// - If the number of values in @p in_vals is greater than the elements addressed
///   by @p slice, then the extra elements in in_vals are ignored.
/// - If the number of values in @p in_vals is less than the elements addressed by
///   by @p slice, then the extra elements address by the @p slice are removed.
/// - This method can be used with an sorted map Object to behave like a sparse array.
///   However, the start and stop must be explicitly specified in the slice.  If any
///   of the slice parameters are negative, a std::out_of_range exception is thrown.
/// @throws std::out_of_range
/// @throws WrongType
/// @throws EmptyReference
inline
void Object::set(const Slice& slice, const Object& obj) {
    set(slice, obj.iter_values());
}

/// Set a slice of values from an iterator.
/// @param slice An interval (slice) of keys.
/// @param in_vals The iterator from which values will be taken.
/// - If the number of values in @p in_vals is greater than the elements addressed
///   by @p slice, then the extra elements in in_vals are ignored.
/// - If the number of values in @p in_vals is less than the elements addressed by
///   by @p slice, then the extra elements address by the @p slice are removed.
/// - This method can be used with an sorted map Object to behave like a sparse array.
///   However, the start and stop must be explicitly specified in the slice.  If any
///   of the slice parameters are negative, a std::out_of_range exception is thrown.
/// @throws std::out_of_range
/// @throws WrongType
/// @throws EmptyReference
inline
void Object::set(const Slice& slice, ValueRange&& in_vals) {
    switch (m_fields.repr_ix) {
        case EMPTY: throw empty_reference();
        case LIST: {
            ObjectList& list = std::get<0>(*m_repr.pl);
            auto indices = slice.to_indices(list.size());
            Int start = std::get<0>(indices);
            Int stop = std::get<1>(indices);
            Int step = std::get<2>(indices);
            if (step == 1) {
                for (auto val : in_vals) {
                    if (val.parent() == nil) val = val.copy();
                    if (start >= stop) {
                        list.insert(list.begin() + start, val);
                    } else {
                        list[start++] = val;
                    }
                    val.set_parent(*this);
                }
                if (start < stop)
                    list.erase(list.begin() + start, list.begin() + stop);
            } else if (step > 1) {
                for (auto val : in_vals) {
                    if (val.parent() != nil) val = val.copy();
                    list[start] = val;
                    val.set_parent(*this);
                    start += step;
                    if (start >= stop) break;
                }
                --step;  // size reduced by one on each iteration
                for (; start < stop && start < (Int)list.size(); start += step) {
                    list.erase(list.begin() + start, list.begin() + start + 1);
                }
            } else if (step == -1) {
                for (auto val : in_vals) {
                    if (val.parent() == nil) val = val.copy();
                    if (start <= stop) {
                        list.insert(list.begin() + start + 1, val);
                    } else {
                        list[start--] = val;
                    }
                    val.set_parent(*this);
                }
                if (start > stop)
                    list.erase(list.begin() + stop + 1, list.begin() + start + 1);
            } else if (step < -1) {
                for (auto val : in_vals) {
                    if (val.parent() != nil) val = val.copy();
                    list[start] = val;
                    val.set_parent(*this);
                    start += step;
                    if (start <= stop) break;
                }
                ++step;  // size reduced by on on each iteration
                for (; start > stop && start >= 0; start += step)
                    list.erase(list.begin() + start, list.begin() + start + 1);
            }
            break;
        }
        case SMAP: {
            auto indices = slice.to_indices(0);
            Int start = std::get<0>(indices);
            Int stop = std::get<1>(indices);
            Int step = std::get<2>(indices);
            if (start < 0 || stop < 0 || step < 0)
                throw std::out_of_range("slice");
            for (auto val : in_vals) {
                if (start >= stop) break;
                set(start, val);
                start += step;
            }
            if (start < stop) {
                for (; start < stop; start += step)
                    del(start);
            }
            break;
        }
        case DSRC: m_repr.ds->set(*this, slice, std::forward<ValueRange>(in_vals)); break;
        default:   throw wrong_type(m_fields.repr_ix);
    }
}

inline
void Object::del(const Slice& slice) {
    switch (m_fields.repr_ix) {
        case EMPTY: throw empty_reference();
        case LIST: {
            auto& list = std::get<0>(*m_repr.pl);
            auto [start, stop, step] = slice.to_indices(list.size());
            if (step == 1) {
                list.erase(list.begin() + start, list.begin() + stop);
            } else if (step > 1) {
                for (; start < stop; ++start)
                    list.erase(list.begin() + start);
            } else if (step < 0) {
                for (; start > stop; --start)
                    list.erase(list.begin() + start);
            }
            break;
        }
        case SMAP: {
            KeyList keys;
            for (auto key : iter_keys(slice))
                keys.push_back(key);
            for (auto key : keys)
                del(key);
            break;
        }
        case DSRC: m_repr.ds->del(*this, slice); break;
        default:   throw wrong_type(m_fields.repr_ix);
    }
}

inline
bool Object::operator == (const Object& obj) const {
    if (is_empty() || obj.is_empty()) throw empty_reference();
    if (is(obj)) return true;

    switch (m_fields.repr_ix) {
        case NIL: {
            return obj.m_fields.repr_ix == NIL;
        }
        case BOOL: {
            switch (obj.m_fields.repr_ix)
            {
                case BOOL:  return m_repr.b == obj.m_repr.b;
                case INT:   return m_repr.b == (bool)obj.m_repr.i;
                case UINT:  return m_repr.b == (bool)obj.m_repr.u;
                case FLOAT: return m_repr.b == (bool)obj.m_repr.f;
                default:    return false;
            }
        }
        case INT: {
            switch (obj.m_fields.repr_ix)
            {
                case BOOL:  return (bool)m_repr.i == obj.m_repr.b;
                case INT:   return m_repr.i == obj.m_repr.i;
                case UINT:  return equal(obj.m_repr.u, m_repr.i);
                case FLOAT: return (Float)m_repr.i == obj.m_repr.f;
                default:    return false;
            }
        }
        case UINT: {
            switch (obj.m_fields.repr_ix)
            {
                case BOOL:  return (bool)m_repr.u == obj.m_repr.b;
                case INT:   return equal(m_repr.u, obj.m_repr.i);
                case UINT:  return m_repr.u == obj.m_repr.u;
                case FLOAT: return (Float)m_repr.u == obj.m_repr.f;
                default:    return false;
            }
        }
        case FLOAT: {
            switch (obj.m_fields.repr_ix)
            {
                case BOOL:  return (bool)m_repr.f == obj.m_repr.b;
                case INT:   return m_repr.f == (Float)obj.m_repr.i;
                case UINT:  return m_repr.f == (Float)obj.m_repr.u;
                case FLOAT: return m_repr.f == obj.m_repr.f;
                default:    return false;
            }
        }
        case STR: {
            if (obj.m_fields.repr_ix == STR) return std::get<0>(*m_repr.ps) == std::get<0>(*obj.m_repr.ps);
            return false;
        }
        case LIST: {
            if (obj.m_fields.repr_ix != LIST) return false;
            auto& lhs = std::get<0>(*m_repr.pl);
            auto& rhs = std::get<0>(*obj.m_repr.pl);
            if (lhs.size() != rhs.size()) return false;
            for (ObjectList::size_type i=0; i<lhs.size(); i++)
                if (lhs[i] != rhs[i])
                    return false;
            return true;
        }
        case SMAP: [[fallthrough]];
        case OMAP: {
            if (!nodel::is_map(obj)) return false;
            if (size() != obj.size()) return false;
            for (const auto& item : iter_items()) {
                if (item.second != obj.get(item.first))
                    return false;
            }
            return true;
        }
        case ANY: {
            return m_repr.pany->equals(obj);
        }
        case DSRC: return m_repr.ds->get_cached(*this) == obj;
        default:     throw wrong_type(m_fields.repr_ix);
    }
}

inline
bool Object::operator == (nil_t) const {
    return m_fields.repr_ix == NIL;
}

inline
std::partial_ordering Object::operator <=> (const Object& obj) const {
    using ordering = std::partial_ordering;

    if (is_empty() || obj.is_empty()) throw empty_reference();
    if (is(obj)) return ordering::equivalent;

    switch (m_fields.repr_ix) {
        case NIL: {
            if (obj.m_fields.repr_ix != NIL)
                throw wrong_type(m_fields.repr_ix);
            return ordering::equivalent;
        }
        case BOOL: {
            if (obj.m_fields.repr_ix != BOOL)
                throw wrong_type(m_fields.repr_ix);
            return m_repr.b <=> obj.m_repr.b;
        }
        case INT: {
            switch (obj.m_fields.repr_ix) {
                case INT:   return m_repr.i <=> obj.m_repr.i;
                case UINT:  return compare(m_repr.i, obj.m_repr.u);
                case FLOAT: return m_repr.i <=> obj.m_repr.f;
                default:
                    throw wrong_type(m_fields.repr_ix);
            }
            break;
        }
        case UINT: {
            switch (obj.m_fields.repr_ix) {
                case INT:   return compare(m_repr.u, obj.m_repr.i);
                case UINT:  return m_repr.u <=> obj.m_repr.u;
                case FLOAT: return m_repr.u <=> obj.m_repr.f;
                default:
                    throw wrong_type(m_fields.repr_ix);
            }
        }
        case FLOAT: {
            switch (obj.m_fields.repr_ix) {
                case INT:   return m_repr.f <=> obj.m_repr.i;
                case UINT:  return m_repr.f <=> obj.m_repr.u;
                case FLOAT: return m_repr.f <=> obj.m_repr.f;
                default:
                    throw wrong_type(m_fields.repr_ix);
            }
            break;
        }
        case STR: {
            if (obj.m_fields.repr_ix != STR)
                throw wrong_type(m_fields.repr_ix);
            return std::get<0>(*m_repr.ps) <=> std::get<0>(*obj.m_repr.ps);
        }
        case DSRC: {
            auto result = obj <=> m_repr.ds->get_cached(*this);
            if (result == std::partial_ordering::less) return std::partial_ordering::greater;
            if (result == std::partial_ordering::greater) return std::partial_ordering::less;
            return result;
        }
        default:     throw wrong_type(m_fields.repr_ix);
    }
}



class WalkDF
{
public:
    static constexpr uint8_t FIRST_VALUE = 0x0;
    static constexpr uint8_t NEXT_VALUE = 0x1;
    static constexpr uint8_t BEGIN_PARENT = 0x2;
    static constexpr uint8_t END_PARENT = 0x4;

    using Item = std::tuple<Object, Key, Object, uint8_t>;
    using Stack = std::stack<Item>;
    using VisitFunc = std::function<void(const Object&, const Key&, const Object&, uint8_t)>;

public:
    WalkDF(Object root, VisitFunc visitor)
    : m_visitor{visitor}
    {
        if (root.is_empty()) throw root.empty_reference();
        m_stack.emplace(Object(), 0, root, FIRST_VALUE);
    }

    bool next() {
        bool has_next = !m_stack.empty();
        if (has_next) {
            const auto [parent, key, object, event] = m_stack.top();
            m_stack.pop();

            if (event & END_PARENT) {
                m_visitor(parent, key, object, event);
            } else {
                switch (object.m_fields.repr_ix) {
                    case Object::LIST: {
                        m_visitor(parent, key, object, event | BEGIN_PARENT);
                        m_stack.emplace(parent, key, object, event | END_PARENT);
                        auto& list = std::get<0>(*object.m_repr.pl);
                        Int index = list.size() - 1;
                        for (auto it = list.crbegin(); it != list.crend(); it++, index--) {
                            m_stack.emplace(object, index, *it, (index == 0)? FIRST_VALUE: NEXT_VALUE);
                        }
                        break;
                    }
                    case Object::SMAP: {
                        m_visitor(parent, key, object, event | BEGIN_PARENT);
                        m_stack.emplace(parent, key, object, event | END_PARENT);
                        auto& map = std::get<0>(*object.m_repr.psm);
                        Int index = map.size() - 1;
                        for (auto it = map.crbegin(); it != map.crend(); it++, index--) {
                            auto& [key, child] = *it;
                            m_stack.emplace(object, key, child, (index == 0)? FIRST_VALUE: NEXT_VALUE);
                        }
                        break;
                    }
                    case Object::OMAP: {
                        m_visitor(parent, key, object, event | BEGIN_PARENT);
                        m_stack.emplace(parent, key, object, event | END_PARENT);
                        auto& map = std::get<0>(*object.m_repr.pom);
                        Int index = map.size() - 1;
                        for (auto it = map.rcbegin(); it != map.rcend(); it++, index--) {
                            auto& [key, child] = *it;
                            m_stack.emplace(object, key, child, (index == 0)? FIRST_VALUE: NEXT_VALUE);
                        }
                        break;
                    }
                    case Object::DSRC: {
                        m_stack.emplace(parent, key, object.m_repr.ds->get_cached(object), FIRST_VALUE);
                        break;
                    }
                    default: {
                        m_visitor(parent, key, object, event);
                        break;
                    }
                }
            }
        }
        return has_next;
    }

private:
    VisitFunc m_visitor;
    Stack m_stack;
};


class WalkBF
{
public:
    using Item = std::tuple<Object, Key, Object>;
    using Deque = std::deque<Item>;
    using VisitFunc = std::function<void(const Object&, const Key&, const Object&)>;

public:
    WalkBF(Object root, VisitFunc visitor)
    : m_visitor{visitor}
    {
        if (root.is_empty()) throw root.empty_reference();
        m_deque.emplace_back(Object(), 0, root);
    }

    bool next() {
        bool has_next = !m_deque.empty();
        if (has_next) {
            const auto [parent, key, object] = m_deque.front();
            m_deque.pop_front();

            switch (object.m_fields.repr_ix) {
                case Object::LIST: {
                    auto& list = std::get<0>(*object.m_repr.pl);
                    Int index = 0;
                    for (auto it = list.cbegin(); it != list.cend(); it++, index++) {
                        const auto& child = *it;
                        m_deque.emplace_back(object, index, child);
                    }
                    break;
                }
                case Object::SMAP: {
                    auto& map = std::get<0>(*object.m_repr.psm);
                    for (auto it = map.cbegin(); it != map.cend(); it++) {
                        auto& [key, child] = *it;
                        m_deque.emplace_back(object, key, child);
                    }
                    break;
                }
                case Object::OMAP: {
                    auto& map = std::get<0>(*object.m_repr.pom);
                    for (auto it = map.cbegin(); it != map.cend(); it++) {
                        auto& [key, child] = *it;
                        m_deque.emplace_back(object, key, child);
                    }
                    break;
                }
                case Object::DSRC: {
                    m_deque.emplace_front(parent, key, object.m_repr.ds->get_cached(object));
                    break;
                }
                default: {
                    m_visitor(parent, key, object);
                    break;
                }
            }
        }
        return has_next;
    }

private:
    VisitFunc m_visitor;
    Deque m_deque;
};


inline
String Object::to_json() const {
    StringStream ss;
    to_json(ss);
    return ss.str();
}

inline
void Object::to_json(std::ostream& os) const {
    auto visitor = [&os] (const Object& parent, const Key& key, const Object& object, uint8_t event) -> void {
        if (event & WalkDF::NEXT_VALUE && !(event & WalkDF::END_PARENT)) {
            os << ", ";
        }

        if (nodel::is_map(parent) && !(event & WalkDF::END_PARENT)) {
            os << key.to_json() << ": ";
        }

        switch (object.m_fields.repr_ix) {
            case EMPTY: throw empty_reference();
            case NIL:   os << "nil"; break;
            case BOOL:  os << (object.m_repr.b? "true": "false"); break;
            case INT:   os << int_to_str(object.m_repr.i); break;
            case UINT:  os << int_to_str(object.m_repr.u); break;
            case FLOAT: os << float_to_str(object.m_repr.f); break;
            case STR:   os << std::quoted(std::get<0>(*object.m_repr.ps)); break;
            case LIST:  os << ((event & WalkDF::BEGIN_PARENT)? '[': ']'); break;
            case SMAP:  [[fallthrough]];
            case OMAP:  os << ((event & WalkDF::BEGIN_PARENT)? '{': '}'); break;
            case ANY:   os << object.m_repr.pany->repr(); break;
            default:    throw wrong_type(object.m_fields.repr_ix);
        }
    };

    WalkDF walk{*this, visitor};
    while (walk.next());
}

inline
Oid Object::id() const {
    auto repr_ix = m_fields.repr_ix;
    switch (repr_ix) {
        case EMPTY: throw empty_reference();
        case NIL:   return Oid::nil();
        case BOOL:  return Oid{repr_ix, m_repr.b};
        case INT:   return Oid{repr_ix, *((uint64_t*)(&m_repr.i))};
        case UINT:  return Oid{repr_ix, m_repr.u};;
        case FLOAT: return Oid{repr_ix, *((uint64_t*)(&m_repr.f))};;
        case STR:   return Oid{repr_ix, (uint64_t)(&(std::get<0>(*m_repr.ps)))};
        case LIST:  return Oid{repr_ix, (uint64_t)(&(std::get<0>(*m_repr.pl)))};
        case SMAP:  return Oid{repr_ix, (uint64_t)(&(std::get<0>(*m_repr.pom)))};
        case OMAP:  return Oid{repr_ix, (uint64_t)(&(std::get<0>(*m_repr.pom)))};
        case ANY:   return Oid{repr_ix, (uint64_t)(m_repr.pany)};
        case DSRC:  return Oid{repr_ix, (uint64_t)(m_repr.ds)};
        default:      throw wrong_type(m_fields.repr_ix);
    }
}

inline
bool Object::is(const Object& other) const {
    auto repr_ix = m_fields.repr_ix;
    if (other.m_fields.repr_ix != repr_ix) return false;
    switch (repr_ix) {
        case EMPTY: throw empty_reference();
        case NIL:  return true;
        case BOOL:  return m_repr.b == other.m_repr.b;
        case INT:   return m_repr.i == other.m_repr.i;
        case UINT:  return m_repr.u == other.m_repr.u;
        case FLOAT: return m_repr.f == other.m_repr.f;
        case STR:   return m_repr.ps == other.m_repr.ps;
        case LIST:  return m_repr.pl == other.m_repr.pl;
        case SMAP:  return m_repr.psm == other.m_repr.psm;
        case OMAP:  return m_repr.pom == other.m_repr.pom;
        case ANY:   return m_repr.pany == other.m_repr.pany;
        case DSRC:  return m_repr.ds == other.m_repr.ds;
        default:      throw wrong_type(m_fields.repr_ix);
    }
}

inline
Object Object::copy() const {
    switch (m_fields.repr_ix) {
        case EMPTY: throw empty_reference();
        case NIL:  return NIL;
        case BOOL:  return m_repr.b;
        case INT:   return m_repr.i;
        case UINT:  return m_repr.u;
        case FLOAT: return m_repr.f;
        case STR:   return std::get<0>(*m_repr.ps);
        case LIST:  return std::get<0>(*m_repr.pl);   // TODO: long-hand deep-copy, instead of using stack
        case SMAP:  return std::get<0>(*m_repr.psm);  // TODO: long-hand deep-copy, instead of using stack
        case OMAP:  return std::get<0>(*m_repr.pom);  // TODO: long-hand deep-copy, instead of using stack
        case ANY:   return m_repr.pany->clone();
        case DSRC:  return m_repr.ds->copy(*this, DataSource::Origin::MEMORY);
        case DEL:   return DEL;
        default:    throw wrong_type(m_fields.repr_ix);
    }
}

inline
refcnt_t Object::ref_count() const {
    switch (m_fields.repr_ix) {
        case STR:   return std::get<1>(*m_repr.ps).m_fields.ref_count;
        case LIST:  return std::get<1>(*m_repr.pl).m_fields.ref_count;
        case SMAP:  return std::get<1>(*m_repr.psm).m_fields.ref_count;
        case OMAP:  return std::get<1>(*m_repr.pom).m_fields.ref_count;
        case ANY:   return m_repr.pany->get_parent().m_fields.ref_count;
        case DSRC:  return m_repr.ds->ref_count();
        case ERROR: return std::get<1>(*m_repr.ps).m_fields.ref_count;
        default:    return no_ref_count;
    }
}


inline
void Object::inc_ref_count() const {
    Object& self = *const_cast<Object*>(this);
    switch (m_fields.repr_ix) {
        case EMPTY: throw empty_reference();
        case STR:
            //DEBUG("inc_ref_count({}:{:x}) -> {}", type_name(), (uint64_t)(self.m_repr.ps), std::get<1>(*self.m_repr.ps).m_fields.ref_count + 1);
            ++(std::get<1>(*self.m_repr.ps).m_fields.ref_count); break;
        case LIST:
            //DEBUG("inc_ref_count({}:{:x}) -> {}", type_name(), (uint64_t)(self.m_repr.pl), std::get<1>(*self.m_repr.pl).m_fields.ref_count + 1);
            ++(std::get<1>(*self.m_repr.pl).m_fields.ref_count); break;
        case SMAP:
            //DEBUG("inc_ref_count({}:{:x}) -> {}", type_name(), (uint64_t)(self.m_repr.psm), std::get<1>(*self.m_repr.psm).m_fields.ref_count + 1);
            ++(std::get<1>(*self.m_repr.psm).m_fields.ref_count); break;
        case OMAP:
            //DEBUG("inc_ref_count({}:{:x}) -> {}", type_name(), (uint64_t)(self.m_repr.pom), std::get<1>(*self.m_repr.pom).m_fields.ref_count + 1);
            ++(std::get<1>(*self.m_repr.pom).m_fields.ref_count); break;
        case ANY:
            //DEBUG("inc_ref_count({}:{:x}) -> {}", type_name(), (uint64_t)(self.m_repr.pany), self.m_repr.pany->get_parent().m_fields.ref_count + 1);
            ++(self.m_repr.pany->get_parent().m_fields.ref_count); break;
        case DSRC:  m_repr.ds->inc_ref_count(); break;
        case ERROR: ++(std::get<1>(*self.m_repr.ps).m_fields.ref_count); break;
        default:    break;
    }
}

inline
void Object::dec_ref_count() const {
    Object& self = *const_cast<Object*>(this);
    switch (m_fields.repr_ix) {
        case STR: {
            auto p_irc = self.m_repr.ps;
            //DEBUG("dec_ref_count({}:{:x}) -> {}", type_name(), (uint64_t)(p_irc), std::get<1>(*p_irc).m_fields.ref_count - 1);
            if (--(std::get<1>(*p_irc).m_fields.ref_count) == 0) {
                std::get<1>(*p_irc).m_fields.repr_ix = NIL;  // clear parent
                delete p_irc;
            }
            break;
        }
        case LIST: {
            auto p_irc = self.m_repr.pl;
            //DEBUG("dec_ref_count({}:{:x}) -> {}", type_name(), (uint64_t)(p_irc), std::get<1>(*p_irc).m_fields.ref_count - 1);
            if (--(std::get<1>(*p_irc).m_fields.ref_count) == 0) {
                std::get<1>(*p_irc).m_fields.repr_ix = NIL;  // clear parent
                for (auto& value : std::get<0>(*p_irc))
                    value.clear_parent();
                delete p_irc;
            }
            break;
        }
        case SMAP: {
            auto p_irc = self.m_repr.psm;
            //DEBUG("dec_ref_count({}:{:x}) -> {}", type_name(), (uint64_t)(p_irc), std::get<1>(*p_irc).m_fields.ref_count - 1);
            if (--(std::get<1>(*p_irc).m_fields.ref_count) == 0) {
                std::get<1>(*p_irc).m_fields.repr_ix = NIL;  // clear parent
                for (auto& item : std::get<0>(*p_irc))
                    item.second.clear_parent();
                delete p_irc;
            }
            break;
        }
        case OMAP: {
            auto p_irc = self.m_repr.pom;
            //DEBUG("dec_ref_count({}:{:x}) -> {}", type_name(), (uint64_t)(p_irc), std::get<1>(*p_irc).m_fields.ref_count - 1);
            if (--(std::get<1>(*p_irc).m_fields.ref_count) == 0) {
                std::get<1>(*p_irc).m_fields.repr_ix = NIL;  // clear parent
                for (auto& item : std::get<0>(*p_irc))
                    item.second.clear_parent();
                delete p_irc;
            }
            break;
        }
        case ANY: {
            auto p_opaque = self.m_repr.pany;
            //DEBUG("dec_ref_count({}:{:x}) -> {}", type_name(), (uint64_t)(p_opaque), p_opaque->get_parent().m_fields.ref_count - 1);
            if (--(p_opaque->get_parent().m_fields.ref_count) == 0) {
                p_opaque->get_parent().m_fields.repr_ix = NIL;
                delete p_opaque;
            }
            break;
        }
        case DSRC: {
            auto* p_ds = self.m_repr.ds;
            if (p_ds->dec_ref_count() == 0) {
                delete p_ds;
            }
            break;
        }
        case ERROR: {
            auto p_irc = self.m_repr.ps;
            if (--(std::get<1>(*p_irc).m_fields.ref_count) == 0) {
                std::get<1>(*p_irc).m_fields.repr_ix = NIL;  // clear parent (should never be set)
                delete p_irc;
            }
            break;
        }
        default:
            break;
    }
}

inline
void Object::release() {
    dec_ref_count();
    m_repr.z = nullptr;
    m_fields.repr_ix = EMPTY;
}

inline
Object& Object::operator = (const Object& other) {
    dec_ref_count();

    m_fields.repr_ix = other.m_fields.repr_ix;
    switch (m_fields.repr_ix) {
        case EMPTY: m_repr.z = nullptr; break;
        case NIL:   m_repr.z = nullptr; break;
        case BOOL:  m_repr.b = other.m_repr.b; break;
        case INT:   m_repr.i = other.m_repr.i; break;
        case UINT:  m_repr.u = other.m_repr.u; break;
        case FLOAT: m_repr.f = other.m_repr.f; break;
        case STR: {
            m_repr.ps = other.m_repr.ps;
            inc_ref_count();
            break;
        }
        case LIST: {
            m_repr.pl = other.m_repr.pl;
            inc_ref_count();
            break;
        }
        case SMAP: {
            m_repr.psm = other.m_repr.psm;
            inc_ref_count();
            break;
        }
        case OMAP: {
            m_repr.pom = other.m_repr.pom;
            inc_ref_count();
            break;
        }
        case ANY: {
            m_repr.pany = other.m_repr.pany;
            inc_ref_count();
            break;
        }
        case DSRC: {
            m_repr.ds = other.m_repr.ds;
            inc_ref_count();
            break;
        }
        case DEL: {
            m_repr.z = nullptr;
            break;
        }
        case ERROR: {
            m_repr.ps = other.m_repr.ps;
            inc_ref_count();
            break;
        }
        default:
            throw wrong_type(m_fields.repr_ix);
    }
    return *this;
}

inline
Object& Object::operator = (Object&& other) {
    dec_ref_count();

    m_fields.repr_ix = other.m_fields.repr_ix;
    switch (m_fields.repr_ix) {
        case EMPTY: m_repr.z = nullptr; break;
        case NIL:   m_repr.z = nullptr; break;
        case BOOL:  m_repr.b = other.m_repr.b; break;
        case INT:   m_repr.i = other.m_repr.i; break;
        case UINT:  m_repr.u = other.m_repr.u; break;
        case FLOAT: m_repr.f = other.m_repr.f; break;
        case STR:   m_repr.ps = other.m_repr.ps; break;
        case LIST:  m_repr.pl = other.m_repr.pl; break;
        case SMAP:  m_repr.psm = other.m_repr.psm; break;
        case OMAP:  m_repr.pom = other.m_repr.pom; break;
        case ANY:   m_repr.pany = other.m_repr.pany; break;
        case DSRC:  m_repr.ds = other.m_repr.ds; break;
        case DEL:   m_repr.z = nullptr; break;
        case ERROR: m_repr.ps = other.m_repr.ps; break;
        default:    throw wrong_type(m_fields.repr_ix);
    }

    other.m_fields.repr_ix = EMPTY;
    other.m_repr.z = nullptr; // safe, but may not be necessary

    return *this;
}

template <class DataSourceType>
DataSourceType* Object::data_source() const {
    if (m_fields.repr_ix != DSRC) return nullptr;
    return dynamic_cast<DataSourceType*>(m_repr.ds);
}


class AncestorIterator
{
  public:
    AncestorIterator(const Object& object) : m_object{object} {}
    AncestorIterator(Object&& object) : m_object{std::forward<Object>(object)} {}

    AncestorIterator(const AncestorIterator&) = default;
    AncestorIterator(AncestorIterator&&) = default;

    AncestorIterator& operator ++ () {
        m_object.refer_to(m_object.parent());
        if (m_object == nil) m_object.release();
        return *this;
    }
    Object operator * () const { return m_object; }
    bool operator == (const AncestorIterator& other) const {
        // m_object may be empty - see AncestorRange::end
        if (m_object.is_empty()) return other.m_object.is_empty();
        return !other.m_object.is_empty() && m_object.is(other.m_object);
    }
  private:
    Object m_object;
};


class AncestorRange
{
  public:
    AncestorRange(const Object& object) : m_object{object} {}
    AncestorIterator begin() const { return m_object; }
    AncestorIterator end() const   { return Object{}; }
  private:
    Object m_object;
};

/// Iterate this Object and its ancestors
inline
AncestorRange Object::iter_ancestor() const {
    return *this;
}


template <class ValueType, typename VisitPred, typename EnterPred>
class TreeIterator
{
  public:
    using TreeRange = Object::TreeRange<ValueType, VisitPred, EnterPred>;

  public:
    TreeIterator() = default;  // python creates, then assigns

    TreeIterator(TreeRange* p_range) : mp_range{p_range} {}

    TreeIterator(TreeRange* p_range, ObjectList& list)
      : mp_range{p_range}
      , m_it{list.begin()}
      , m_end{list.end()}
    {
        mp_range->fifo().push_back(Object{nil});  // dummy
        enter(list[0]);
        if (!visit(*m_it)) ++(*this);
    }

    TreeIterator& operator ++ () {
        auto& fifo = mp_range->fifo();

        while (1) {
            ++m_it;

            while (m_it == m_end) {
                fifo.pop_front();
                if (fifo.empty()) break;
                m_it = fifo.front().begin();
                m_end = fifo.front().end();
            }

            if (m_it == m_end) break;

            const Object& obj = *m_it;
            enter(obj);
            if (visit(obj)) break;
        }
        return *this;
    }

    Object operator * () const { return *m_it; }
    bool operator == (const TreeIterator& other) const { return mp_range->fifo().empty(); }

  private:
    bool visit(const Object& obj) {
        if constexpr (std::is_invocable<VisitPred, const Object&>::value) {
            return mp_range->should_visit(obj);
        } else {
            return true;
        }
    }

    void enter(const Object& obj) {
        if constexpr (std::is_invocable<EnterPred, const Object&>::value) {
            if (nodel::is_container(obj) && mp_range->should_enter(obj))
                mp_range->fifo().push_back(obj);
        } else {
            if (nodel::is_container(obj))
                mp_range->fifo().push_back(obj);
        }
    }

  private:
    TreeRange* mp_range;
    ValueIterator m_it;
    ValueIterator m_end;
};


template <class ValueType, typename VisitPred, typename EnterPred>
class Object::TreeRange
{
  public:
    using Iterator = TreeIterator<ValueType, VisitPred, EnterPred>;

  public:
    TreeRange() = default;  // python creates, then assigns

    TreeRange(const Object& object, VisitPred&& visit_pred, EnterPred&& enter_pred)
      : m_visit_pred{visit_pred}
      , m_enter_pred{enter_pred}
    {
        m_list.push_back(object);
    }

    Iterator begin() { return TreeIterator{this, m_list}; }
    Iterator end()   { return TreeIterator{this}; }

    bool should_visit(const Object& obj) const { return !m_visit_pred || m_visit_pred(obj); }
    bool should_enter(const Object& obj) const { return !m_enter_pred || m_enter_pred(obj); }
    auto& fifo() { return m_fifo; }

  private:
    ObjectList m_list;
    std::deque<ValueRange> m_fifo;
    VisitPred m_visit_pred;
    EnterPred m_enter_pred;
};

/// Iterate the subtree rooted on this Object.
/// @return Returns a range-like object.
inline
Object::TreeRange<Object, NoPredicate, NoPredicate> Object::iter_tree() const {
    return {*this, NoPredicate{}, NoPredicate{}};
}

/// Iterate the subtree rooted on this Object and filtered by a predicate.
/// @tparam VisitPred Any function-like type (lambdas)
/// @param visit_pred A predicate function that is called with each iterated Object.
/// @return Returns a range-like object.
template <typename VisitPred>
Object::TreeRange<Object, Predicate, NoPredicate> Object::iter_tree(VisitPred&& visit_pred) const {
    return {*this, std::forward<Predicate>(visit_pred), NoPredicate{}};
}

/// Iterate selected branches of the subtree rooted on this Object.
/// @tparam EnterPred Any function-like type (lambdas)
/// @param enter_pred A predicate function that determines whether the algorithm descends into
///        a container Object.
/// Unlike the "visit" predicate, the "enter" predicate controls which Objects in the
/// subtree are entered (descended into). This is useful for improving performance,
/// as well as controlling whether Objects with DataSources are loaded into memory.
/// @return Returns a range-like object.
template <typename EnterPred>
Object::TreeRange<Object, NoPredicate, Predicate> Object::iter_tree_if(EnterPred&& enter_pred) const {
    return {*this, NoPredicate{}, std::forward<Predicate>(enter_pred)};
}

/// Iterate with both a "visit" predicate and an "enter" predicate.
/// @tparam VisitPred Any function-like type (lambdas)
/// @tparam EnterPred Any function-like type (lambdas)
/// @param visit_pred A predicate function that is called with each iterated Object.
/// @param enter_pred A predicate function that determines whether the algorithm descends into
///        a container Object.
/// @return Returns a range-like object.
template <typename VisitPred, typename EnterPred>
Object::TreeRange<Object, Predicate, Predicate> Object::iter_tree_if(VisitPred&& visit_pred, EnterPred&& enter_pred) const {
    return {*this, std::forward<Predicate>(visit_pred), std::forward<Predicate>(enter_pred)};
}

/// Mark this Object as having been updated
/// - If this Object has a DataSoource then it is flagged as having been updated
///   such that it will be saved on the next call to @ref Object::save. This is only
///   necessary when string data is modified by reference by calling
///   @ref Object::as<T>
inline
void Object::needs_saving() {
    auto p_ds = data_source<DataSource>();
    if (p_ds != nullptr) p_ds->m_unsaved = true;
}

/// Save changes made in the subtree
/// - This method visits Objects with a DataSource in the subtree of this Object
///   that have been loaded into memory and calls the @ref DataSource::save method.
/// - This method will *not* trigger a DataSource to load.
/// - Use the @ref Object::reset or @ref Object::reset_key methods to discard
///   changes before calling this method.
/// - Only Objects that whose *unsaved* bit is set will be saved. The *unsaved* bit
///   is set whenever a method is called that modifies the Object's data.
/// - Use the @ref Object::needs_saving method to set the *unsaved* bit, if you
///   modify string data by reference, as returned by the @ref Object::as<String>
///   method.
inline
void Object::save() {
    auto enter_pred = [] (const Object& obj) {
        auto p_ds = obj.data_source<DataSource>();
        return p_ds != nullptr && p_ds->is_multi_level() && p_ds->is_fully_cached();
    };

    auto tree_range = iter_tree_if(has_data_source, enter_pred);
    for (const auto& obj : tree_range)
        obj.m_repr.ds->save(obj);
}

inline
void DataSource::report_read_error(std::string&& error) {
    if (m_options.throw_read_error) throw DataSourceError{std::forward<std::string>(error)};
    m_read_failed = true;
    if (!m_options.quiet_read) WARN(error);
}

inline
void DataSource::report_write_error(std::string&& error) {
    if (m_options.throw_write_error) throw DataSourceError{std::forward<std::string>(error)};
    if (!m_options.quiet_write) WARN(error);
}

inline
bool DataSource::is_valid(const Object& target) {
    if (is_sparse()) {
        if (m_cache.is_empty()) read_type(target);
        return !m_read_failed;
    } else {
        insure_fully_cached(target);
        return !m_read_failed;
    }
}

inline
void Object::reset() {
    if (m_fields.repr_ix == DSRC)
        m_repr.ds->reset();
}

inline
void Object::reset_key(const Key& key) {
    if (m_fields.repr_ix == DSRC)
        m_repr.ds->reset_key(key);
}

inline
void Object::refresh() {
    if (m_fields.repr_ix == DSRC)
        m_repr.ds->refresh();
}

inline
void Object::refresh_key(const Key& key) {
    if (m_fields.repr_ix == DSRC)
        m_repr.ds->refresh_key(key);
}

inline
std::ostream& operator<< (std::ostream& ostream, const Object& obj) {
    ostream << obj.to_str();
    return ostream;
}

inline
void DataSource::Options::configure(const Object& uri) {
    mode = Mode::READ;
    auto query_mode = uri.get("query.perm"_path);
    if (query_mode != nil) {
        auto mode_s = query_mode.as<String>();
        if (mode_s.find_first_of("rR") != mode_s.npos) mode |= Mode::READ;
        if (mode_s.find_first_of("wW") != mode_s.npos) mode |= Mode::WRITE;
        if (mode_s.find_first_of("cC") != mode_s.npos) mode |= Mode::CLOBBER;
    }
}

/////////////////////////////////////////////////////////////////////////////
/// Bind an object to a DataSource.
/// @param obj The object to be bound.
/// - If the object has a parent, the object is first removed from the
///   parent, bound to the DataSource, and then added back to the parent.
/// - The object must not already be bound to another DataSource.
/// - This is a low-level method that should not be called directly by users.
/// - The behavior is undefined (probably terrible) if the DataSource is
///   already bound to another object.
/// - A WrongType exception is thrown if the DataSource requires a specific
///   object type, and the object argument has a different type.
/////////////////////////////////////////////////////////////////////////////
inline
void DataSource::bind(Object& obj) {
    if (m_repr_ix != Object::EMPTY && m_repr_ix != obj.m_fields.repr_ix)
        throw Object::wrong_type(obj.m_fields.repr_ix);

    Key key;
    Object parent = obj.parent();
    if (parent != nil) {
        key = parent.key_of(obj);
        obj.clear_parent();
    }

    m_cache = obj;
    m_fully_cached = true;
    m_unsaved = true;

    obj = this;

    if (parent != nil) {
        parent.set(key, obj);
    }
}

inline
const Object& DataSource::get_cached(const Object& target) const {
    const_cast<DataSource*>(this)->insure_fully_cached(target);
    return m_cache;
}

inline
size_t DataSource::size(const Object& target) {
    if (is_sparse()) {
        std::unordered_set<Key> deleted;
        auto range = m_cache.iter_items();
        std::for_each(range.begin(), range.end(), [&deleted] (const auto& item) {
            if (item.second.is_deleted())
                deleted.insert(item.first);
        });

        auto opt_it = key_iter();
        assert (opt_it);
        size_t size = 0;
        auto& it = *opt_it;
        for (; !it.done(); it.next()) {
            if (!deleted.contains(it.key())) {
                ++size;
            }
        }

        return size;
    } else {
        insure_fully_cached(target);
        return m_cache.size();
    }
}

inline
Object DataSource::get(const Object& target, const Key& key) {
    if (is_sparse()) {
        if (m_cache.is_empty()) { read_type(target); }

        Object value = m_cache.get(key);
        switch (value.m_fields.repr_ix) {
            case Object::NIL: {
                value = read_key(target, key);
                read_set(target, key, value);
                break;
            }
            case Object::DEL: {
                value = nil;
            }
            default:
                break;
        }

        return value;
    } else {
        insure_fully_cached(target);
        return m_cache.get(key);
    }
}

inline
DataSource::Mode DataSource::resolve_mode() const {
    Mode mode = m_options.mode;
    if (mode & Mode::INHERIT && m_parent.m_fields.repr_ix == Object::DSRC) {
        mode = m_parent.m_repr.ds->resolve_mode();
    }
    return mode;
}

inline
void DataSource::update_parent_in_cache(const Object& parent) {
    if (!m_cache.is_empty() && is_container(m_cache)) {
        for (auto& obj : m_cache.iter_values())
            obj.set_parent(parent);
    }
}

inline
DataSource* DataSource::copy(const Object& target, Origin origin) const {
    auto* p_dsrc = new_instance(target, origin);
    p_dsrc->m_cache = get_cached(target).copy();
    return p_dsrc;
}

inline
void DataSource::set(const Object& target, const Object& value) {
    // target is guaranteed not to have a parent
    Mode mode = resolve_mode();
    if (!(mode & Mode::WRITE)) throw WriteProtect();
    if (is_sparse() && !(mode & Mode::CLOBBER)) throw ClobberProtect();

    set_unsaved(true);
    m_cache = value;
    m_fully_cached = true;
}

inline
Object DataSource::set(const Object& target, const Key& key, const Object& in_val) {
    Mode mode = resolve_mode();
    if (!(mode & Mode::WRITE)) throw WriteProtect();

    set_unsaved(true);
    if (is_sparse()) {
        if (m_cache.is_empty()) read_type(target);
        auto out_val = m_cache.set(key, in_val);
        out_val.set_parent(target);
        return out_val;
    } else {
        insure_fully_cached(target);
        auto out_val = m_cache.set(key, in_val);
        out_val.set_parent(target);
        return out_val;
    }
}

inline
Object DataSource::set(const Object& target, Key&& key, const Object& in_val) {
    Mode mode = resolve_mode();
    if (!(mode & Mode::WRITE)) throw WriteProtect();

    set_unsaved(true);
    if (is_sparse()) {
        if (m_cache.is_empty()) read_type(target);
        auto out_val = m_cache.set(std::forward<Key>(key), in_val);
        out_val.set_parent(target);
        return out_val;
    } else {
        insure_fully_cached(target);
        auto out_val = m_cache.set(std::forward<Key>(key), in_val);
        out_val.set_parent(target);
        return out_val;
    }
}

inline
void DataSource::set(const Object& target, const Slice& slice, ValueRange&& in_vals) {
    Mode mode = resolve_mode();
    if (!(mode & Mode::WRITE)) throw WriteProtect();

    set_unsaved(true);
    if (is_sparse()) {
        if (m_cache.is_empty()) read_type(target);
    } else {
        insure_fully_cached(target);
    }
    m_cache.set(slice, std::forward<ValueRange>(in_vals));
    for (auto val : m_cache.get(slice))
        val.set_parent(target);
}

inline
void DataSource::del(const Object& target, const Key& key) {
    Mode mode = resolve_mode();
    if (!(mode & Mode::WRITE)) throw WriteProtect();

    set_unsaved(true);
    if (is_sparse()) {
        if (m_cache.is_empty()) read_type(target);
        m_cache.get(key).clear_parent();
        m_cache.set(key, Object::DEL);
    } else {
        insure_fully_cached(target);
        m_cache.del(key);
    }
}

// TODO: allow DataSource to optimize slice deletes
inline
void DataSource::del(const Object& target, const Slice& slice) {
    Mode mode = resolve_mode();
    if (!(mode & Mode::WRITE)) throw WriteProtect();

    set_unsaved(true);
    if (is_sparse()) {
        if (m_cache.is_empty()) read_type(target);
        for (auto& obj : m_cache.get(slice))
            obj.clear_parent();
        // TODO: memory overhead does not scale for large intervals
        for (auto key : m_cache.iter_keys(slice))
            m_cache.set(key, Object::DEL);
    } else {
        insure_fully_cached(target);
        m_cache.del(slice);
    }
}

inline
void DataSource::clear(const Object& target) {
    if (is_sparse()) throw DataSourceError("Sparse data-sources cannot be cleared.");

    Mode mode = resolve_mode();
    if (!(mode & Mode::WRITE)) throw WriteProtect();

    insure_fully_cached(target);
    m_cache.clear();
    set_unsaved(true);
}

inline
void DataSource::save(const Object& target) {
    if (!(resolve_mode() & Mode::WRITE)) throw WriteProtect();
    if (m_cache.is_empty()) return;

    if (m_unsaved) {
        m_write_failed = false;

        if (m_fully_cached) {
            write(target, m_cache);

            if (is_sparse()) {
                // final save notification to support batching
                commit(target, m_cache, {});
            }
        }
        else if (is_sparse()) {
            KeyList deleted_keys;

            for (auto& [key, value] : m_cache.iter_items()) {
                if (value.m_fields.repr_ix == Object::DEL) {
                    delete_key(target, key);
                    deleted_keys.push_back(key);
                } else {
                    write_key(target, key, value);
                }
            }

            // clear delete records
            for (auto& del_key : deleted_keys)
                m_cache.del(del_key);

            // final save notification to support batching
            commit(target, m_cache, deleted_keys);
        }

        if (!m_write_failed)
            m_unsaved = false;
    }
}

inline
void DataSource::reset() {
    free_resources();
    m_fully_cached = false;
    m_unsaved = false;
    m_read_failed = false;
    m_write_failed = false;
    m_cache = Object{m_repr_ix};
}

inline
void DataSource::reset_key(const Key& key) {
    if (is_sparse()) {
        if (!m_cache.is_empty())
            m_cache.del(key);
    } else {
        reset();
    }
}

inline
void DataSource::refresh() {
    // - If not cached, noop
    if (!m_fully_cached) return;
    // - Reload data for this level
    // - Insert new keys
    // - Update matching keys without data-sources
    // - Enqueue for refresh matching keys with data-sources
}

inline
void DataSource::refresh_key(const Key& key) {
    // TODO: implement refresh
}

inline
void DataSource::insure_fully_cached(const Object& target) {
    if (!m_fully_cached) {
//        DEBUG("Loading: {}", target.path().to_str());
        read(target);

        m_fully_cached = true;

        // NOTE: These are the only types that are allowed a nested level of caching
        switch (m_cache.m_fields.repr_ix) {
            case Object::LIST: {
                for (auto& value : std::get<0>(*m_cache.m_repr.pl))
                    value.set_parent(target);  // TODO: avoid setting parent twice
                break;
            }
            case Object::SMAP: {
                for (auto& item : std::get<0>(*m_cache.m_repr.psm))
                    item.second.set_parent(target);  // TODO: avoid setting parent twice
                break;
            }
            case Object::OMAP: {
                for (auto& item : std::get<0>(*m_cache.m_repr.pom))
                    item.second.set_parent(target);  // TODO: avoid setting parent twice
                break;
            }
            default:
                break;
        }
    }
}

inline
void DataSource::read_set(const Object& target, const Object& value) {
    ASSERT(value.type() != Object::ERROR);
    // TODO: Have data-source return all keys at once, so we know whether we're performing
    // a fresh load, or a refresh.
    m_cache.set(value);
}

inline
void DataSource::read_set(const Object& target, const Key& key, const Object& value) {
    ASSERT(value.type() != Object::ERROR);
    m_cache.set(key, value);
    value.set_parent(target);
}

inline
void DataSource::read_set(const Object& target, Key&& key, const Object& value) {
    ASSERT(value.type() != Object::ERROR);
    m_cache.set(std::forward<Key>(key), value);
    value.set_parent(target);
}

inline
void DataSource::read_del(const Object& target, const Key& key) {
    m_cache.del(key);
}


namespace test { template <typename T> bool is_resolved(Object::Subscript<T>& subscript); }

template <class AccessType>
class Object::Subscript
{
  public:
    Object& resolve() const {
        auto& self = const_cast<Object&>(m_obj);
        if (m_pend) {
            self = m_obj.get(m_sub);
            m_pend = false;
        }
        return self;
    }

  public:
    Subscript(const Object& obj, const AccessType& sub) : m_obj{obj}, m_sub{sub}, m_pend{true} {}

    ReprIX type() const                { return resolve().type(); }
    std::string_view type_name() const { return resolve().type_name(); }

    Object root() const   { return resolve().root(); }
    Object parent() const { return resolve().parent(); }

    KeyRange iter_keys() const     { return resolve().iter_keys(); }
    ItemRange iter_items() const   { return resolve().iter_items(); }
    ValueRange iter_values() const { return resolve().iter_values(); }
    AncestorRange iter_ancestor() const    { return resolve().iter_ancestor(); }

    KeyRange iter_keys(const Slice& slice) const     { return resolve().iter_keys(slice); }
    ItemRange iter_items(const Slice& slice) const   { return resolve().iter_items(slice); }
    ValueRange iter_values(const Slice& slice) const { return resolve().iter_values(slice); }

    TreeRange<Object, NoPredicate, NoPredicate> iter_tree() const { return resolve().iter_tree(); }

    template <typename VisitPred>
    TreeRange<Object, Predicate, NoPredicate> iter_tree(VisitPred&& visit_pred) const {
        return resolve().iter_tree(std::forward<VisitPred>(visit_pred));
    }

    template <typename EnterPred>
    TreeRange<Object, NoPredicate, Predicate> iter_tree_if(EnterPred&& enter_pred) const {
        return resolve().iter_tree_if(std::forward<EnterPred>(enter_pred));
    }

    template <typename VisitPred, typename EnterPred>
    TreeRange<Object, Predicate, Predicate> iter_tree_if(VisitPred&& visit_pred, EnterPred&& enter_pred) const {
        return resolve().iter_tree_if(std::forward<VisitPred>(visit_pred), std::forward<EnterPred>(enter_pred));
    }

    const KeyList keys() const      { return resolve().keys(); }
    const ItemList items() const    { return resolve().items(); }
    const ObjectList values() const { return resolve().values(); }
    size_t size() const             { return resolve().size(); }

    const Key key() const                     { if constexpr (std::is_same<AccessType, Key>::value) return m_sub; else return m_sub.tail(); }
    const Key key_of(const Object& obj) const { return resolve().key_of(obj); }
    OPath path() const                        { return resolve().path(); }
    OPath path(const Object& root) const      { return resolve().path(root); }

    template <typename T> bool is_type() const { return resolve().template is_type<T>(); }

    template <typename V> void visit(V&& visitor) const { return resolve().visit(visitor); }

    template <typename T> T as() const requires is_byvalue<T>                         { return resolve().template as<T>(); }
    template <typename T> const T& as() const requires std::is_same<T, String>::value { return resolve().template as<T>(); }
    template <typename T> T& as() requires is_byvalue<T>                              { return resolve().template as<T>(); }
    template <typename T> T& as() requires std::is_same<T, String>::value             { return resolve().template as<T>(); }

    bool is_empty() const   { return resolve().is_empty(); }
    bool is_deleted() const { return resolve().is_deleted(); }
    bool is_valid() const     { return resolve().is_valid(); }

    Key to_key() const                       { return resolve().to_key(); }
    String to_str() const                    { return resolve().to_str(); }
    String to_json() const                   { return resolve().to_json(); }
    void to_json(std::ostream& stream) const { return resolve().to_json(stream); }

    Object get(is_like_Int auto v) const     { return resolve().get(v); }
    Object get(const Key& key) const         { return resolve().get(key); }
    Object get(const OPath& path) const      { return resolve().get(path); }
    ObjectList get(const Slice& slice) const { return resolve().get(slice); }

    Object set(const Object& obj)                    { return resolve().set(obj); }
    Object set(const Key& key, const Object& obj)    { return resolve().set(key, obj); }
    Object set(const OPath& key, const Object& obj)  { return resolve().set(key, obj); }
    void set(const Slice& slice, const Object& obj)  { return resolve().set(slice, obj); }
    void set(const Slice& slice, ValueRange&& iter) { return resolve().set(slice, std::forward<ValueRange>(iter)); }
    Object insert(const Key& key, const Object& obj) { return resolve().insert(key, obj); }

    void del(const Key& key)     { resolve().del(key); }
    void del(const OPath& path)  { resolve().del(path); }
    void del(const Slice& slice) { resolve().del(slice); }
    void del_from_parent()       { if (m_pend) m_obj.del(m_sub); else m_obj.del_from_parent(); }
    void clear()                 { resolve().clear(); }

    Subscript<OPath> operator [] (const Key& key);
    Subscript<OPath> operator [] (const OPath& path);

    bool operator == (const Object& other) const                   { return resolve() == other; }
    bool operator == (nil_t) const                                 { return resolve().type() == Object::NIL; }
    std::partial_ordering operator <=> (const Object& other) const { return resolve() <=> other; }

    Oid id() const                     { return resolve().id(); }
    bool is(const Object& other) const { return resolve().is(other); }
    Object copy() const                { return resolve().copy(); }
    refcnt_t ref_count() const         { return resolve().ref_count(); }

    Object operator = (const Object& obj) { return m_obj.set(m_sub, obj); }
    Object operator = (Object&& other)    { return m_obj.set(m_sub, std::forward<Object>(other)); }

    template <class DataSourceType>
    DataSourceType* data_source() const { return resolve().template data_source<DataSourceType>(); }

    void needs_saving()              { resolve().needs_saving(); }
    void save()                      { resolve().save(); }
    void reset()                     { resolve().reset(); }
    void reset_key(const Key& key)   { resolve().reset_key(key); }
    void refresh()                   { resolve().refresh(); }
    void refresh_key(const Key& key) { resolve().refresh_key(key); }

  private:
    Object m_obj;
    AccessType m_sub;
    mutable bool m_pend;

  template <typename T> friend bool test::is_resolved(Subscript<T>& subscript);
};


inline Object::Object(const Subscript<Key>& sub) : Object{sub.resolve()} {}
inline Object::Object(const Subscript<OPath>& sub) : Object{sub.resolve()} {}

inline Object::Subscript<Key> Object::operator [] (const Key& key)      { return {*this, key}; }
inline Object::Subscript<OPath> Object::operator [] (const OPath& path) { return {*this, path}; }

template <class T>
Object::Subscript<OPath> Object::Subscript<T>::operator [] (const Key& key) {
    if (m_pend) return {m_obj, OPath{m_sub, key}};
    OPath path;
    path.append(key);
    return {m_obj, path};
}

template <class T>
Object::Subscript<OPath> Object::Subscript<T>::operator [] (const OPath& path) {
    if (m_pend) return {m_obj, OPath{m_sub, path}};
    return {m_obj, path};
}

inline
std::ostream& operator<< (std::ostream& ostream, const Object::Subscript<Key>& sub) {
    ostream << sub.resolve().to_str();
    return ostream;
}

inline
std::ostream& operator<< (std::ostream& ostream, const Object::Subscript<OPath>& sub) {
    ostream << sub.resolve().to_str();
    return ostream;
}

inline
Object make_error(const String& message) {
    Object obj = Object::ERROR;
    obj.as<String>() = message;
    return obj;
}

inline
Object make_error(String&& message) {
    Object obj = Object::ERROR;
    obj.as<String>().swap(message);
    return obj;
}

namespace test {

class DataSourceTestInterface
{
  public:
    DataSourceTestInterface(DataSource& data_source) : m_data_source{data_source} {}
    Object& cache() { return m_data_source.m_cache; }
  private:
    DataSource& m_data_source;
};


template <typename T> bool is_resolved(Object::Subscript<T>& subscript) { return !subscript.m_pend; }

/// @example examples/basic.cpp
/// This is an example showing how to create an Object from JSON and access keys.

/// @example examples/find.cpp
/// This is an example of using an Object bound to the filesystem to perform a
/// recursive directory search similar to the unix `find` command.

/// @example examples/rocksdb.cpp
/// This is an example of creating a RocksDB database from an Object, iterate
/// of all the keys in the database, and iterate over a range of the keys.
/// To manage memory overhead, the Object is periodically saved and reset.
/// A Stopwatch instance is used to report latencies.

} // test namespace
} // nodel namespace


namespace std {

//////////////////////////////////////////////////////////////////////////////
/// OPath hash function support
//////////////////////////////////////////////////////////////////////////////
template<>
struct hash<nodel::OPath>
{
    std::size_t operator () (const nodel::OPath& path) const noexcept {
      return path.hash();
    }
};

} // namespace std
