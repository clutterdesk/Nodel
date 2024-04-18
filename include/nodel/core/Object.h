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

#include <limits>
#include <string>
#include <string_view>
#include <vector>
#include <map>
#include <tsl/ordered_map.h>
#include <unordered_set>
#include <stack>
#include <deque>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <functional>
#include <cstdint>
#include <ctype.h>
#include <errno.h>
#include <memory>
#include <type_traits>

#include "Oid.h"
#include "Key.h"
#include "Slice.h"

#include <nodel/support/Flags.h>
#include <nodel/support/logging.h>
#include <nodel/support/string.h>
#include <nodel/support/exception.h>
#include <nodel/support/parse.h>
#include <nodel/support/types.h>


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
class OPath;
class DataSource;
class KeyIterator;
class ValueIterator;
class ItemIterator;
class KeyRange;
class ValueRange;
class ItemRange;
class LineRange;

struct NoPredicate {};
using Predicate = std::function<bool(const Object&)>;

using String = std::string;
using List = std::vector<Object>;
using SortedMap = std::map<Key, Object>;
using OrderedMap = tsl::ordered_map<Key, Object, KeyHash>;

using Item = std::pair<Key, Object>;
using ConstItem = std::pair<const Key, const Object>;
using KeyList = std::vector<Key>;
using ItemList = std::vector<Item>;

// inplace reference count types (ref-count stored in parent bit-field)
using IRCString = std::tuple<String, Object>;
using IRCList = std::tuple<List, Object>;
using IRCMap = std::tuple<SortedMap, Object>;
using IRCOMap = std::tuple<OrderedMap, Object>;

using IRCStringPtr = IRCString*;
using IRCListPtr = IRCList*;
using IRCMapPtr = IRCMap*;
using IRCOMapPtr = IRCOMap*;
using DataSourcePtr = DataSource*;

namespace test { class DataSourceTestInterface; }

constexpr size_t min_key_chunk_size = 128;


//////////////////////////////////////////////////////////////////////////////
/// @brief Nodel dynamic object.
//////////////////////////////////////////////////////////////////////////////
class Object
{
  public:
    template <class T> class Subscript;
    template <class ValueType, typename VisitPred, typename EnterPred> class TreeRange;

    enum ReprIX {
        EMPTY,   // uninitialized reference
        NIL,     // json null, and used to indicate non-existence
        BOOL,
        INT,
        UINT,
        FLOAT,
        STR,
        LIST,
        MAP,     // sorted map
        OMAP,    // ordered map
        DSRC,    // DataSource
        DEL,     // indicates deleted key in sparse data-store
        INVALID = 31
    };

  public:
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
    template <typename T> ReprIX get_repr_ix() const;
    template <> ReprIX get_repr_ix<bool>() const       { return BOOL; }
    template <> ReprIX get_repr_ix<Int>() const        { return INT; }
    template <> ReprIX get_repr_ix<UInt>() const       { return UINT; }
    template <> ReprIX get_repr_ix<Float>() const      { return FLOAT; }
    template <> ReprIX get_repr_ix<String>() const     { return STR; }
    template <> ReprIX get_repr_ix<List>() const       { return LIST; }
    template <> ReprIX get_repr_ix<SortedMap>() const  { return MAP; }
    template <> ReprIX get_repr_ix<OrderedMap>() const { return OMAP; }

    template <typename T> T get_repr() const requires is_byvalue<T>;
    template <> bool get_repr<bool>() const     { return m_repr.b; }
    template <> Int get_repr<Int>() const       { return m_repr.i; }
    template <> UInt get_repr<UInt>() const     { return m_repr.u; }
    template <> Float get_repr<Float>() const   { return m_repr.f; }

    template <typename T> T& get_repr() requires is_byvalue<T>;
    template <> bool& get_repr<bool>()     { return m_repr.b; }
    template <> Int& get_repr<Int>()       { return m_repr.i; }
    template <> UInt& get_repr<UInt>()     { return m_repr.u; }
    template <> Float& get_repr<Float>()   { return m_repr.f; }

    template <typename T> const T& get_repr() const requires std::is_same<T, String>::value { return std::get<0>(*m_repr.ps); }
    template <typename T> T& get_repr() requires std::is_same<T, String>::value             { return std::get<0>(*m_repr.ps); }

  private:
    struct NoParent {};
    Object(NoParent&&) : m_repr{}, m_fields{NIL} {}  // initialize reference count

  public:
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
          case MAP:   return "sorted-map";
          case OMAP:  return "ordered-map";
          case DSRC:  return "data-source";
          case DEL:   return "deleted";
          case INVALID: return "invalid";
          default:    return "<undefined>";
      }
    }

  public:
    static constexpr refcnt_t no_ref_count = std::numeric_limits<refcnt_t>::max();

    Object()                           : m_repr{}, m_fields{EMPTY} {}
    Object(nil_t)                      : m_repr{}, m_fields{NIL} {}
    Object(const String& str)          : m_repr{new IRCString{str, NoParent()}}, m_fields{STR} {}
    Object(String&& str)               : m_repr{new IRCString(std::forward<String>(str), NoParent())}, m_fields{STR} {}
    Object(const StringView& sv)       : m_repr{new IRCString{{sv.data(), sv.size()}, NoParent()}}, m_fields{STR} {}
    Object(const char* v)              : m_repr{new IRCString{v, NoParent()}}, m_fields{STR} { ASSERT(v != nullptr); }
    Object(bool v)                     : m_repr{v}, m_fields{BOOL} {}
    Object(is_like_Float auto v)       : m_repr{(Float)v}, m_fields{FLOAT} {}
    Object(is_like_Int auto v)         : m_repr{(Int)v}, m_fields{INT} {}
    Object(is_like_UInt auto v)        : m_repr{(UInt)v}, m_fields{UINT} {}
    Object(DataSourcePtr ptr);

    Object(const List&);
    Object(const SortedMap&);
    Object(const OrderedMap&);
    Object(List&&);
    Object(SortedMap&&);
    Object(OrderedMap&&);

    Object(const Key&);
    Object(Key&&);

    Object(ReprIX type);

    Object(const Object& other);
    Object(Object&& other);

    ~Object() { dec_ref_count(); }

    ReprIX type() const;
    std::string_view type_name() const { return type_name(type()); }

    Object root() const;
    Object parent() const;

    KeyRange iter_keys() const;
    ItemRange iter_items() const;
    ValueRange iter_values() const;
    LineRange iter_line() const;

    KeyRange iter_keys(const Slice&) const;
    ItemRange iter_items(const Slice&) const;
    ValueRange iter_values(const Slice&) const;

    TreeRange<Object, NoPredicate, NoPredicate> iter_tree() const;

    template <typename VisitPred>
    TreeRange<Object, Predicate, NoPredicate> iter_tree(VisitPred&&) const;

    template <typename EnterPred>
    TreeRange<Object, NoPredicate, Predicate> iter_tree_if(EnterPred&&) const;

    template <typename VisitPred, typename EnterPred>
    TreeRange<Object, Predicate, Predicate> iter_tree_if(VisitPred&&, EnterPred&&) const;

    const KeyList keys() const;
    const ItemList items() const;
    const List values() const;

    size_t size() const;

    const Key key() const;
    const Key key_of(const Object&) const;
    OPath path() const;
    OPath path(const Object& root) const;

    template <typename T>
    T value_cast() const;

    template <typename T>
    bool is_type() const { return resolve_repr_ix() == get_repr_ix<T>(); }

    template <typename V>
    void visit(V&& visitor) const;

    template <typename T>
    T as() const requires is_byvalue<T> {
        if (m_fields.repr_ix == get_repr_ix<T>()) return get_repr<T>();
        else if (m_fields.repr_ix == DSRC) return dsrc_read().as<T>();
        throw wrong_type(m_fields.repr_ix);
    }

    template <typename T>
    const T& as() const requires std::is_same<T, String>::value {
        if (m_fields.repr_ix == get_repr_ix<T>()) return get_repr<T>();
        else if (m_fields.repr_ix == DSRC) return dsrc_read().as<T>();
        throw wrong_type(m_fields.repr_ix);
    }

    template <typename T>
    T& as() requires is_byvalue<T> {
        if (m_fields.repr_ix == get_repr_ix<T>()) return get_repr<T>();
        else if (m_fields.repr_ix == DSRC) return dsrc_read().as<T>();
        throw wrong_type(m_fields.repr_ix);
    }

    template <typename T>
    T& as() requires std::is_same<T, String>::value {
        if (m_fields.repr_ix == get_repr_ix<T>()) return get_repr<T>();
        else if (m_fields.repr_ix == DSRC) return dsrc_read().as<T>();
        throw wrong_type(m_fields.repr_ix);
    }

    bool is_empty() const   { return m_fields.repr_ix == EMPTY; }
    bool is_deleted() const { return m_fields.repr_ix == DEL; }

    bool is_num() const       { auto type = resolve_repr_ix(); return type == INT || type == UINT || type == FLOAT; }
    bool is_any_int() const   { auto type = resolve_repr_ix(); return type == INT || type == UINT; }
    bool is_map() const       { auto type = resolve_repr_ix(); return type == MAP || type == OMAP; }
    bool is_container() const { auto type = resolve_repr_ix(); return type == LIST || type == OMAP || type == MAP; }

    bool is_valid() const;

    bool to_bool() const;
    Int to_int() const;
    UInt to_uint() const;
    Float to_float() const;
    String to_str() const;

    Key to_key() const;
    Key into_key();
    String to_json() const;
    void to_json(std::ostream&) const;

    Object get(is_like_Int auto v) const;
    Object get(const Key& key) const;
    Object get(const OPath& path) const;
    Object get(const Slice& interval) const;

    Object set(const Object&);
    Object set(const Key&, const Object&);
    Object set(Key&&, const Object&);
    Object set(const OPath&, const Object&);
    void set(const Slice&, const Object&);

    void del(const Key& key);
    void del(const OPath& path);
    void del(const Slice& interval);
    void del_from_parent();

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
    void map_set_slice(SortedMap& map, is_integral auto start, const Object& in_vals);
    Object list_set(const Key& key, const Object& in_val);

    int resolve_repr_ix() const;
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
  friend class LineRange;
  friend class KeyRange;
  friend class ValueRange;
  friend class ItemRange;
  friend struct python::Support;

  friend class WalkDF;
  friend class WalkBF;

  template <class ValueType, typename VisitPred, typename EnterPred> friend class TreeIterator;

  friend bool has_data_source(const Object&);
};


//////////////////////////////////////////////////////////////////////////////
/// @brief A simple path consisting of a list of keys.
/// Path literals can be created using ""_path literal operator.
/// @see nodel::Object::get(const OPath&)
/// @see nodel::Query
//////////////////////////////////////////////////////////////////////////////
class OPath
{
  public:
    using Iterator = KeyList::iterator;
    using ConstIterator = KeyList::const_iterator;

    OPath() {}
    OPath(const StringView& spec);

    OPath(const KeyList& keys) : m_keys{keys} {}
    OPath(KeyList&& keys)      : m_keys{std::forward<KeyList>(keys)} {}

    void append(const Key& key) { m_keys.push_back(key); }
    void append(Key&& key)      { m_keys.emplace_back(std::forward<Key>(key)); }

    OPath parent() const {
        if (m_keys.size() == 0) throw InvalidPath();
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

    void reverse() { std::reverse(m_keys.begin(), m_keys.end()); }
    KeyList keys() { return m_keys; }

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
/// @brief Base class for objects implementing external data access.
//////////////////////////////////////////////////////////////////////////////
class DataSource
{
  public:
    class KeyIterator
    {
      public:
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

    struct Mode : public Flags<uint8_t> {
        FLAG8 READ = 1;
        FLAG8 WRITE = 2;
        FLAG8 CLOBBER = 4;
        FLAG8 ALL = 7;
        FLAG8 INHERIT = 8;
        Mode(Flags<uint8_t> flags) : Flags<uint8_t>(flags) {}
        operator int () const { return m_value; }
    };

    struct Options {
        Options() = default;
        Options(Mode mode) : mode{mode} {}

        /// @brief Configure options from the specified URI query.
        void configure(const Object& uri);

        /// @brief Access control for get/set/del methods
        /// CLOBBER access controls whether a bound Object may be completely overwritten with
        /// a single call to Object::set(const Object&).
        Mode mode = Mode::READ | Mode::WRITE;

        /// @brief Logging control during read operations
        bool quiet_read = false;

        /// @brief Logging control during write operations
        bool quiet_write = false;

        /// @brief Throw exception when read operation fails
        bool throw_read_error = true;

        /// @brief Throw exception when write operation fails
        bool throw_write_error = true;
    };

    DataSource(Kind kind, Options options, Origin origin)
      : m_parent{Object::NIL}
      , m_cache{Object::EMPTY}
      , m_kind{kind}
      , m_repr_ix{Object::EMPTY}
      , m_fully_cached{(kind == Kind::COMPLETE) && origin == Origin::MEMORY}
      , m_unsaved{origin == Origin::MEMORY}
      , m_options{options}
    {}

    DataSource(Kind kind, Options options, ReprIX repr_ix, Origin origin)
      : m_parent{Object::NIL}
      , m_cache{repr_ix}
      , m_kind{kind}
      , m_repr_ix{repr_ix}
      , m_fully_cached{(kind == Kind::COMPLETE) && origin == Origin::MEMORY}
      , m_unsaved{origin == Origin::MEMORY}
      , m_options{options}
    {}

    virtual ~DataSource() {}

  protected:
    virtual DataSource* new_instance(const Object& target, Origin origin) const = 0;
    virtual void configure(const Object& uri)                                {}
    virtual void read_type(const Object& target)                             {}  // define type/id of object
    virtual void read(const Object& target) = 0;
    virtual Object read_key(const Object& target, const Key&)                { return {}; }  // sparse
    virtual void write(const Object& target, const Object&)                  {}  // both
    virtual void write_key(const Object& target, const Key&, const Object&)  {}  // sparse
    virtual void delete_key(const Object& target, const Key&)                {}  // sparse
    virtual void commit(const Object& target, const KeyList& del_keys)       {}  // sparse

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

    void bind(Object& obj);

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

    void report_read_error(std::string&& error);
    void report_write_error(std::string&& error);
    bool is_valid(const Object& target);

  private:
    DataSource* copy(const Object& target, Origin origin) const;
    void set(const Object& target, const Object& in_val);
    Object set(const Object& target, const Key& key, const Object& in_val);
    Object set(const Object& target, Key&& key, const Object& in_val);
    void set(const Object& target, const Slice& key, const Object& in_vals);
    void del(const Object& target, const Key& key);
    void del(const Object& target, const Slice& slice);

    void save(const Object& target);

    void reset();
    void reset_key(const Key& key);
    void refresh();
    void refresh_key(const Key& key);

    String to_str(const Object& target) {
        insure_fully_cached(target);
        return m_cache.to_str();
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
  friend Object bind(const URI&, const DataSource::Options&, Object);
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
Object::Object(const Object& other) : m_fields{other.m_fields} {
    switch (m_fields.repr_ix) {
        case EMPTY: m_repr.z = nullptr; break;
        case NIL:  m_repr.z = nullptr; break;
        case BOOL:  m_repr.b = other.m_repr.b; break;
        case INT:   m_repr.i = other.m_repr.i; break;
        case UINT:  m_repr.u = other.m_repr.u; break;
        case FLOAT: m_repr.f = other.m_repr.f; break;
        case STR:   m_repr.ps = other.m_repr.ps; inc_ref_count(); break;
        case LIST:  m_repr.pl = other.m_repr.pl; inc_ref_count(); break;
        case MAP:   m_repr.psm = other.m_repr.psm; inc_ref_count(); break;
        case OMAP:  m_repr.pom = other.m_repr.pom; inc_ref_count(); break;
        case DSRC:  m_repr.ds = other.m_repr.ds; m_repr.ds->inc_ref_count(); break;
    }
}

inline
Object::Object(Object&& other) : m_fields{other.m_fields} {
    switch (m_fields.repr_ix) {
        case EMPTY: m_repr.z = nullptr; break;
        case NIL:  m_repr.z = nullptr; break;
        case BOOL:  m_repr.b = other.m_repr.b; break;
        case INT:   m_repr.i = other.m_repr.i; break;
        case UINT:  m_repr.u = other.m_repr.u; break;
        case FLOAT: m_repr.f = other.m_repr.f; break;
        case STR:   m_repr.ps = other.m_repr.ps; break;
        case LIST:  m_repr.pl = other.m_repr.pl; break;
        case MAP:   m_repr.psm = other.m_repr.psm; break;
        case OMAP:  m_repr.pom = other.m_repr.pom; break;
        case DSRC:  m_repr.ds = other.m_repr.ds; break;
    }

    other.m_fields.repr_ix = EMPTY;
    other.m_repr.z = nullptr;
}

inline
Object::Object(DataSourcePtr ptr) : m_repr{ptr}, m_fields{DSRC} {}  // DataSource ownership transferred

inline
Object::Object(const List& list) : m_repr{new IRCList({}, NoParent{})}, m_fields{LIST} {
    auto& my_list = std::get<0>(*m_repr.pl);
    for (auto& value : list) {
        Object copy = value.copy();
        my_list.push_back(copy);
        copy.set_parent(*this);
    }
}

inline
Object::Object(const SortedMap& map) : m_repr{new IRCMap({}, NoParent{})}, m_fields{MAP} {
    auto& my_map = std::get<0>(*m_repr.psm);
    for (auto& [key, value]: map) {
        Object copy = value.copy();
        my_map.insert({key, copy});
        copy.set_parent(*this);
    }
}

inline
Object::Object(const OrderedMap& map) : m_repr{new IRCOMap({}, NoParent{})}, m_fields{OMAP} {
    auto& my_map = std::get<0>(*m_repr.pom);
    for (auto& [key, value]: map) {
        Object copy = value.copy();
        my_map.insert({key, copy});
        copy.set_parent(*this);
    }
}

inline
Object::Object(List&& list) : m_repr{new IRCList(std::forward<List>(list), NoParent{})}, m_fields{LIST} {
    auto& my_list = std::get<0>(*m_repr.pl);
    for (auto& obj : my_list)
        obj.set_parent(*this);
}

inline
Object::Object(SortedMap&& map) : m_repr{new IRCMap(std::forward<SortedMap>(map), NoParent{})}, m_fields{MAP} {
    auto& my_map = std::get<0>(*m_repr.psm);
    for (auto& [key, obj]: my_map)
        const_cast<Object&>(obj).set_parent(*this);
}

inline
Object::Object(OrderedMap&& map) : m_repr{new IRCOMap(std::forward<OrderedMap>(map), NoParent{})}, m_fields{OMAP} {
    auto& my_map = std::get<0>(*m_repr.pom);
    for (auto& [key, obj]: my_map)
        const_cast<Object&>(obj).set_parent(*this);
}

inline
Object::Object(const Key& key) : m_fields{EMPTY} {
    switch (key.m_repr_ix) {
        case Key::NIL:  m_fields.repr_ix = NIL; m_repr.z = nullptr; break;
        case Key::BOOL:  m_fields.repr_ix = BOOL; m_repr.b = key.m_repr.b; break;
        case Key::INT:   m_fields.repr_ix = INT; m_repr.i = key.m_repr.i; break;
        case Key::UINT:  m_fields.repr_ix = UINT; m_repr.u = key.m_repr.u; break;
        case Key::FLOAT: m_fields.repr_ix = FLOAT; m_repr.f = key.m_repr.f; break;
        case Key::STR: {
            m_fields.repr_ix = STR;
            m_repr.ps = new IRCString{String{key.m_repr.s.data()}, {}};
            break;
        }
        default: throw std::invalid_argument("key");
    }
}

inline
Object::Object(Key&& key) : m_fields{EMPTY} {
    switch (key.m_repr_ix) {
        case Key::NIL:  m_fields.repr_ix = NIL; m_repr.z = nullptr; break;
        case Key::BOOL:  m_fields.repr_ix = BOOL; m_repr.b = key.m_repr.b; break;
        case Key::INT:   m_fields.repr_ix = INT; m_repr.i = key.m_repr.i; break;
        case Key::UINT:  m_fields.repr_ix = UINT; m_repr.u = key.m_repr.u; break;
        case Key::FLOAT: m_fields.repr_ix = FLOAT; m_repr.f = key.m_repr.f; break;
        case Key::STR: {
            m_fields.repr_ix = STR;
            m_repr.ps = new IRCString{String{key.m_repr.s.data()}, {}};
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
        case LIST:  m_repr.pl = new IRCList{{}, NoParent{}}; break;
        case MAP:   m_repr.psm = new IRCMap{{}, NoParent{}}; break;
        case OMAP:  m_repr.pom = new IRCOMap{{}, NoParent{}}; break;
        case DEL:   m_repr.z = nullptr; break;
        default:      throw wrong_type(type);
    }
}

inline
Object::ReprIX Object::type() const {
    switch (m_fields.repr_ix) {
        case DSRC: return m_repr.ds->type(*this);
        default:     return (ReprIX)m_fields.repr_ix;
    }
}

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

inline
Object Object::parent() const {
    switch (m_fields.repr_ix) {
        case EMPTY: throw empty_reference();
        case STR:   return std::get<1>(*m_repr.ps);
        case LIST:  return std::get<1>(*m_repr.pl);
        case MAP:   return std::get<1>(*m_repr.psm);
        case OMAP:  return std::get<1>(*m_repr.pom);
        case DSRC:  return m_repr.ds->m_parent;
        default:      return nil;
    }
}

template <typename V>
void Object::visit(V&& visitor) const {
    switch (m_fields.repr_ix) {
        case EMPTY: throw empty_reference();
        case NIL:  visitor(nullptr); break;
        case BOOL:  visitor(m_repr.b); break;
        case INT:   visitor(m_repr.i); break;
        case UINT:  visitor(m_repr.u); break;
        case FLOAT: visitor(m_repr.f); break;
        case STR:   visitor(std::get<0>(*m_repr.ps)); break;
        case DSRC:  visitor(*this); break;  // TODO: How to visit Object with DataSource?
        default:      throw wrong_type(m_fields.repr_ix);
    }
}

inline
int Object::resolve_repr_ix() const {
    switch (m_fields.repr_ix) {
        case DSRC: return const_cast<DataSourcePtr>(m_repr.ds)->type(*this);
        default:     return m_fields.repr_ix;
    }
}

inline
Object Object::dsrc_read() const {
    return const_cast<DataSourcePtr>(m_repr.ds)->get_cached(*this);
}

inline
void Object::refer_to(const Object& other) {
    operator = (other);
}

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
        case MAP:   m_repr.psm = other.m_repr.psm; break;
        case OMAP:  m_repr.pom = other.m_repr.pom; break;
        case DSRC:  m_repr.ds = other.m_repr.ds; break;
        default:      throw wrong_type(m_fields.repr_ix);
    }
}

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
        case MAP: {
            auto& parent = std::get<1>(*m_repr.psm);
            parent.weak_refer_to(new_parent);
            break;
        }
        case OMAP: {
            auto& parent = std::get<1>(*m_repr.pom);
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
        case MAP: {
            auto& parent = std::get<1>(*m_repr.psm);
            parent.m_fields.repr_ix = NIL;
            break;
        }
        case OMAP: {
            auto& parent = std::get<1>(*m_repr.pom);
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

inline
const Key Object::key() const {
    return parent().key_of(*this);
}

inline
const Key Object::key_of(const Object& obj) const {
    switch (m_fields.repr_ix) {
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
        case MAP: {
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

template <typename T>
T Object::value_cast() const {
    switch (m_fields.repr_ix) {
        case BOOL:  return (T)m_repr.b;
        case INT:   return (T)m_repr.i;
        case UINT:  return (T)m_repr.u;
        case FLOAT: return (T)m_repr.f;
        case DSRC:  return m_repr.ds->get_cached(*this).value_cast<T>();
        default:      throw wrong_type(m_fields.repr_ix);
    }
}

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
        case MAP:   [[fallthrough]];
        case OMAP:  return to_json();
        case DSRC:  return const_cast<DataSourcePtr>(m_repr.ds)->to_str(*this);
        default:    throw wrong_type(m_fields.repr_ix);
    }
}

inline
bool Object::is_valid() const {
    if (m_fields.repr_ix == INVALID)
        return false;
    else if (m_fields.repr_ix == DSRC)
        return m_repr.ds->is_valid(*this);
    else
        return true;
}

inline
bool Object::to_bool() const {
    switch (m_fields.repr_ix) {
        case EMPTY: throw empty_reference();
        case NIL:   return false;
        case BOOL:  return m_repr.b;
        case INT:   return m_repr.i;
        case UINT:  return m_repr.u;
        case FLOAT: return m_repr.f;
        case STR:   return str_to_bool(std::get<0>(*m_repr.ps));
        case LIST:  [[fallthrough]];
        case MAP:   [[fallthrough]];
        case OMAP:  return size() > 0;
        case DSRC:  return const_cast<DataSourcePtr>(m_repr.ds)->get_cached(*this).to_bool();
        default:    throw wrong_type(m_fields.repr_ix, BOOL);
    }
}

inline
Int Object::to_int() const {
    switch (m_fields.repr_ix) {
        case EMPTY: throw empty_reference();
        case NIL:   throw wrong_type(m_fields.repr_ix, INT);
        case BOOL:  return m_repr.b;
        case INT:   return m_repr.i;
        case UINT:  return m_repr.u;
        case FLOAT: return m_repr.f;
        case STR:   return str_to_int(std::get<0>(*m_repr.ps));
        case DSRC:  return const_cast<DataSourcePtr>(m_repr.ds)->get_cached(*this).to_int();
        default:    throw wrong_type(m_fields.repr_ix, INT);
    }
}

inline
UInt Object::to_uint() const {
    switch (m_fields.repr_ix) {
        case EMPTY: throw empty_reference();
        case NIL:   throw wrong_type(m_fields.repr_ix, UINT);
        case BOOL:  return m_repr.b;
        case INT:   return m_repr.i;
        case UINT:  return m_repr.u;
        case FLOAT: {
            UInt num = (UInt)m_repr.f;
            return num;
        }
        case STR:   return str_to_int(std::get<0>(*m_repr.ps));
        case DSRC:  return const_cast<DataSourcePtr>(m_repr.ds)->get_cached(*this).to_uint();
        default:      throw wrong_type(m_fields.repr_ix, UINT);
    }
}

inline
Float Object::to_float() const {
    switch (m_fields.repr_ix) {
        case EMPTY: throw empty_reference();
        case NIL:  throw wrong_type(m_fields.repr_ix, BOOL);
        case BOOL:  return m_repr.b;
        case INT:   return m_repr.i;
        case UINT:  return m_repr.u;
        case FLOAT: return m_repr.f;
        case STR:   return str_to_float(std::get<0>(*m_repr.ps));
        case DSRC:  return const_cast<DataSourcePtr>(m_repr.ds)->get_cached(*this).to_float();
        default:      throw wrong_type(m_fields.repr_ix, FLOAT);
    }
}

inline
Key Object::to_key() const {
    switch (m_fields.repr_ix) {
        case EMPTY: throw empty_reference();
        case NIL:  return nil;
        case BOOL:  return m_repr.b;
        case INT:   return m_repr.i;
        case UINT:  return m_repr.u;
        case FLOAT: return m_repr.f;
        case STR:   return std::get<0>(*m_repr.ps);
        case DSRC:  return const_cast<DataSourcePtr>(m_repr.ds)->get_cached(*this).to_key();
        default:      throw wrong_type(m_fields.repr_ix);
    }
}

inline
Key Object::into_key() {
    switch (m_fields.repr_ix) {
        case EMPTY: throw empty_reference();
        case NIL:  return nil;
        case BOOL:  return m_repr.b;
        case INT:   return m_repr.i;
        case UINT:  return m_repr.u;
        case FLOAT: return m_repr.f;
        case STR:   {
            Key k{std::move(std::get<0>(*m_repr.ps))};
            release();
            return k;
        }
        default:
            throw wrong_type(m_fields.repr_ix);
    }
}

inline
bool Object::norm_index(is_like_Int auto& index, UInt size) {
    if (index < 0) index += size;
    if (index < 0 || (UInt)index >= size) return false;
    return true;
}

inline
Object Object::get(is_like_Int auto index) const {
    switch (m_fields.repr_ix) {
        case EMPTY: throw empty_reference();
        case STR: {
            auto& str = std::get<0>(*m_repr.ps);
            if (!norm_index(index, str.size())) return nil;
            return str[index];
        }
        case LIST: {
            auto& list = std::get<0>(*m_repr.pl);
            if (!norm_index(index, list.size())) return nil;
            return list[index];
        }
        case DSRC:  return m_repr.ds->get(*this, index);
        default:      return get(Key{index});
    }
}

inline
Object Object::get(const Key& key) const {
    switch (m_fields.repr_ix) {
        case EMPTY: throw empty_reference();
        case STR: {
            auto& str = std::get<0>(*m_repr.ps);
            if (!key.is_any_int()) throw Key::wrong_type(key.type());
            Int index = key.to_int();
            if (!norm_index(index, str.size())) return nil;
            return str[index];
        }
        case LIST: {
            auto& list = std::get<0>(*m_repr.pl);
            if (!key.is_any_int()) throw Key::wrong_type(key.type());
            Int index = key.to_int();
            if (!norm_index(index, list.size())) return nil;
            return list[index];
        }
        case MAP: {
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
        default:     return nil;
    }
}

inline
Object Object::get(const OPath& path) const {
    return path.lookup(*this);
}

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
Object Object::list_set(const Key& key, const Object& in_val) {
    Object out_val = (in_val.parent() == nil)? in_val: in_val.copy();
    auto& list = std::get<0>(*m_repr.pl);
    if (!key.is_any_int()) throw Key::wrong_type(key.type());
    Int index = key.to_int();
    auto size = list.size();
    if (index < 0) index += size;
    if (index >= (Int)size) {
        list.push_back(out_val);
    } else {
        auto it = list.begin() + index;
        it->set_parent(nil);
        *it = out_val;
    }
    out_val.set_parent(*this);
    return out_val;
}

inline
Object Object::set(const Key& key, const Object& in_val) {
    switch (m_fields.repr_ix) {
        case EMPTY: throw empty_reference();
        case STR: {
            auto& str = std::get<0>(*m_repr.ps);
            if (!key.is_any_int()) throw Key::wrong_type(key.type());
            auto index = key.to_int();
            if (!norm_index(index, str.size())) return nil;
            str.insert(index, in_val.to_str());
            return in_val;
        }
        case LIST: return list_set(key, in_val);
        case MAP: {
            Object out_val = (in_val.parent() == nil)? in_val: in_val.copy();
            auto& map = std::get<0>(*m_repr.psm);
            auto it = map.find(key);
            if (it != map.end()) {
                it->second.set_parent(nil);
                map.erase(it);
            }
            map.insert({key, out_val});
            out_val.set_parent(*this);
            return out_val;
        }
        case OMAP: {
            Object out_val = (in_val.parent() == nil)? in_val: in_val.copy();
            auto& map = std::get<0>(*m_repr.pom);
            auto it = map.find(key);
            if (it != map.end()) {
                it->second.set_parent(nil);
                map.erase(it);
            }
            map.insert({key, out_val});
            out_val.set_parent(*this);
            return out_val;
        }
        case DSRC: return m_repr.ds->set(*this, key, in_val);
        default:     throw wrong_type(m_fields.repr_ix);
    }
}

inline
Object Object::set(Key&& key, const Object& in_val) {
    switch (m_fields.repr_ix) {
        case EMPTY: throw empty_reference();
        case LIST: return list_set(key, in_val);
        case MAP: {
            Object out_val = (in_val.parent() == nil)? in_val: in_val.copy();
            auto& map = std::get<0>(*m_repr.psm);
            auto it = map.find(key);
            if (it != map.end()) {
                it->second.set_parent(nil);
                map.erase(it);
            }
            map.insert({std::forward<Key>(key), out_val});
            out_val.set_parent(*this);
            return out_val;
        }
        case OMAP: {
            Object out_val = (in_val.parent() == nil)? in_val: in_val.copy();
            auto& map = std::get<0>(*m_repr.pom);
            auto it = map.find(key);
            if (it != map.end()) {
                it->second.set_parent(nil);
                map.erase(it);
            }
            map.insert({std::forward<Key>(key), out_val});
            out_val.set_parent(*this);
            return out_val;
        }
        case DSRC: return m_repr.ds->set(*this, std::forward<Key>(key), in_val);
        default:     throw wrong_type(m_fields.repr_ix);
    }
}

inline
Object Object::set(const OPath& path, const Object& in_val) {
    return path.create(*this, in_val);
}

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
        case MAP: {
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

inline
void Object::del(const OPath& path) {
    auto obj = path.lookup(*this);
    if (obj != nil) {
        auto par = obj.parent();
        par.del(par.key_of(obj));
    }
}

inline
void Object::del_from_parent() {
    Object par = parent();
    if (par != nil)
        par.del(par.key_of(*this));
}

inline
size_t Object::size() const {
    switch (m_fields.repr_ix) {
        case EMPTY: throw empty_reference();
        case STR:   return std::get<0>(*m_repr.ps).size();
        case LIST:  return std::get<0>(*m_repr.pl).size();
        case MAP:   return std::get<0>(*m_repr.psm).size();
        case OMAP:  return std::get<0>(*m_repr.pom).size();
        case DSRC:  return m_repr.ds->size(*this);
        default:
            return 0;
    }
}


#include "KeyRange.h"
#include "ValueRange.h"
#include "ItemRange.h"

inline KeyRange Object::iter_keys() const {
    return *this;
}

inline ItemRange Object::iter_items() const {
    return *this;
}

inline ValueRange Object::iter_values() const {
    return *this;
}

inline KeyRange Object::iter_keys(const Slice& slice) const {
    return {*this, slice};
}

inline ItemRange Object::iter_items(const Slice& slice) const {
    return {*this, slice};
}

inline ValueRange Object::iter_values(const Slice& slice) const {
    return {*this, slice};
}

// intended for small intervals - use iter_values
// TODO: consider whether slicing a map should slice keys are slice the list of map items
//       currently it slices keys, which is useful when a map is used to represent a sparse list
inline
Object Object::get(const Slice& slice) const {
    if (m_fields.repr_ix == STR) {
        auto& str = std::get<0>(*m_repr.ps);
        auto [start, stop, step] = slice.to_indices(str.size());
        return get_slice(str, start, stop, step);
    } else if (m_fields.repr_ix == LIST) {
        auto& list = std::get<0>(*m_repr.pl);
        auto [start, stop, step] = slice.to_indices(list.size());
        return get_slice(list, start, stop, step);
    } else {
        List result;
        for (auto value : iter_values(slice))
            result.push_back(value);
        return result;
    }
}

inline
void Object::map_set_slice(SortedMap& map, is_integral auto start, const Object& in_vals) {
    for (auto val : in_vals.iter_values())
        set(start++, val);
}

inline
void Object::set(const Slice& slice, const Object& in_vals) {
    if (in_vals.size() == 0) return;

    switch (m_fields.repr_ix) {
        case EMPTY: throw empty_reference();
        case STR: {
            if (in_vals.type() == STR) {
                auto& str = std::get<0>(*m_repr.ps);
                if (!in_vals.is_type<String>()) throw wrong_type(in_vals.type());
                const auto& repl = in_vals.as<String>();
                auto [start, stop, step] = slice.to_indices(str.size());
                if (step == 1) {
                    str.replace(str.begin() + start, str.begin() + stop, repl.begin(), repl.end());
                } else {
                    set_slice(str, start, stop, step, repl);
                }
            } else {
                throw wrong_type(in_vals.type());
            }
            break;
        }
        case LIST: {
            auto& list = std::get<0>(*m_repr.pl);
            auto [start, stop, step] = slice.to_indices(list.size());
            auto val_list = in_vals.values();
            if (step == 1) {
                list.erase(list.begin() + start, list.begin() + stop);
                list.insert(list.begin() + start, val_list.begin(), val_list.end());
                auto it = list.cbegin() + start;
                auto end = list.cbegin() + start + val_list.size();
                for (; it != end; ++it)
                    it->set_parent(*this);
            } else {
                set_slice(list, start, stop, step, val_list);
            }
            break;
        }
        case MAP: {
            auto& map = std::get<0>(*m_repr.psm);
            del(slice);
            auto start = slice.min().value();
            switch (start.type()) {
                case Key::INT:  map_set_slice(map, start.to_int(), in_vals); break;
                case Key::UINT: map_set_slice(map, start.to_uint(), in_vals); break;
                default:        throw wrong_type(start.type());
            }
            break;
        }
        case DSRC: m_repr.ds->set(*this, slice, in_vals); break;
        default:   throw wrong_type(m_fields.repr_ix);
    }
}

inline
void Object::del(const Slice& slice) {
    switch (m_fields.repr_ix) {
        case EMPTY: throw empty_reference();
        case STR: {
            auto& str = std::get<0>(*m_repr.ps);
            auto [start, stop, step] = slice.to_indices(str.size());
            // TODO: handle step
            str.erase(str.begin() + start, str.begin() + stop);
            break;
        }
        case LIST: {
            auto& list = std::get<0>(*m_repr.pl);
            auto [start, stop, step] = slice.to_indices(list.size());
            // TODO: handle step
            list.erase(list.begin() + start, list.begin() + stop);
            break;
        }
        case MAP: {
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
const KeyList Object::keys() const {
    KeyList keys;
    for (auto key : iter_keys())
        keys.push_back(key);
    return keys;
}

inline
const List Object::values() const {
    List children;
    switch (m_fields.repr_ix) {
        case LIST:  {
            for (auto& child : std::get<0>(*m_repr.pl))
                children.push_back(child);
            break;
        }
        case MAP:  {
            for (auto& item : std::get<0>(*m_repr.psm))
                children.push_back(item.second);
            break;
        }
        case OMAP:  {
            for (auto& item : std::get<0>(*m_repr.pom))
                children.push_back(item.second);
            break;
        }
        case DSRC: {
            auto& dsrc = *m_repr.ds;
            if (dsrc.is_sparse()) {
                ValueRange range{*this};
                for (const auto& value : range)
                    children.push_back(value);
            } else {
                return dsrc.get_cached(*this).values();
            }
            break;
        }
        default:
            throw wrong_type(m_fields.repr_ix);
    }
    return children;
}

inline
const ItemList Object::items() const {
    ItemList items;
    switch (m_fields.repr_ix) {
        case MAP:  {
            for (auto& item : std::get<0>(*m_repr.psm))
                items.push_back(item);
            break;
        }
        case OMAP:  {
            for (auto& item : std::get<0>(*m_repr.pom))
                items.push_back(item);
            break;
        }
        case DSRC: {
            auto& dsrc = *m_repr.ds;
            if (dsrc.is_sparse()) {
                ItemRange range{*this};
                for (const auto& item : range)
                    items.push_back(item);
            } else {
                return dsrc.get_cached(*this).items();
            }
            break;
        }
        default:
            throw wrong_type(m_fields.repr_ix);
    }
    return items;
}

inline
bool Object::operator == (const Object& obj) const {
    if (is_empty() || obj.is_empty()) throw empty_reference();
    if (is(obj)) return true;

    switch (m_fields.repr_ix) {
        case NIL: {
            if (obj.m_fields.repr_ix == NIL) return true;
        }
        case BOOL: {
            switch (obj.m_fields.repr_ix)
            {
                case BOOL:  return m_repr.b == obj.m_repr.b;
                case INT:   return m_repr.b == obj.m_repr.i;
                case UINT:  return m_repr.b == obj.m_repr.u;
                case FLOAT: return m_repr.b == obj.m_repr.f;
                default:    return false;
            }
        }
        case INT: {
            switch (obj.m_fields.repr_ix)
            {
                case BOOL:  return m_repr.i == obj.m_repr.b;
                case INT:   return m_repr.i == obj.m_repr.i;
                case UINT:  return equal(obj.m_repr.u, m_repr.i);
                case FLOAT: return m_repr.i == obj.m_repr.f;
                default:    return false;
            }
        }
        case UINT: {
            switch (obj.m_fields.repr_ix)
            {
                case BOOL:  return m_repr.u == obj.m_repr.b;
                case INT:   return equal(m_repr.u, obj.m_repr.i);
                case UINT:  return m_repr.u == obj.m_repr.u;
                case FLOAT: return m_repr.u == obj.m_repr.f;
                default:    return false;
            }
        }
        case FLOAT: {
            switch (obj.m_fields.repr_ix)
            {
                case BOOL:  return m_repr.f == obj.m_repr.b;
                case INT:   return m_repr.f == obj.m_repr.i;
                case UINT:  return m_repr.f == obj.m_repr.u;
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
            for (List::size_type i=0; i<lhs.size(); i++)
                if (lhs[i] != rhs[i])
                    return false;
            return true;
        }
        case MAP: [[fallthrough]];
        case OMAP: {
            if (!obj.is_map()) return false;
            if (size() != obj.size()) return false;
            if (keys() != obj.keys()) return false;
            for (const auto& item : iter_items()) {
                if (item.second != obj.get(item.first))
                    return false;
            }
            return true;
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
                    case Object::MAP: {
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
                case Object::MAP: {
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

        if (parent.is_map() && !(event & WalkDF::END_PARENT)) {
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
            case MAP:   [[fallthrough]];
            case OMAP:  os << ((event & WalkDF::BEGIN_PARENT)? '{': '}'); break;
            default:      throw wrong_type(object.m_fields.repr_ix);
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
        case NIL:  return Oid::nil();
        case BOOL:  return Oid{repr_ix, m_repr.b};
        case INT:   return Oid{repr_ix, *((uint64_t*)(&m_repr.i))};
        case UINT:  return Oid{repr_ix, m_repr.u};;
        case FLOAT: return Oid{repr_ix, *((uint64_t*)(&m_repr.f))};;
        case STR:   return Oid{repr_ix, (uint64_t)(&(std::get<0>(*m_repr.ps)))};
        case LIST:  return Oid{repr_ix, (uint64_t)(&(std::get<0>(*m_repr.pl)))};
        case MAP:   return Oid{repr_ix, (uint64_t)(&(std::get<0>(*m_repr.pom)))};
        case OMAP:  return Oid{repr_ix, (uint64_t)(&(std::get<0>(*m_repr.pom)))};
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
        case MAP:   return m_repr.psm == other.m_repr.psm;
        case OMAP:  return m_repr.pom == other.m_repr.pom;
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
        case LIST:  return std::get<0>(*m_repr.pl);  // TODO: long-hand deep-copy, instead of using stack
        case MAP:   return std::get<0>(*m_repr.psm);  // TODO: long-hand deep-copy, instead of using stack
        case OMAP:  return std::get<0>(*m_repr.pom);  // TODO: long-hand deep-copy, instead of using stack
        case DSRC:  return m_repr.ds->copy(*this, DataSource::Origin::MEMORY);
        case DEL:   return DEL;
        default:      throw wrong_type(m_fields.repr_ix);
    }
}

inline
refcnt_t Object::ref_count() const {
    switch (m_fields.repr_ix) {
        case STR:   return std::get<1>(*m_repr.ps).m_fields.ref_count;
        case LIST:  return std::get<1>(*m_repr.pl).m_fields.ref_count;
        case MAP:   return std::get<1>(*m_repr.psm).m_fields.ref_count;
        case OMAP:  return std::get<1>(*m_repr.pom).m_fields.ref_count;
        case DSRC:  return m_repr.ds->ref_count();
        default:      return no_ref_count;
    }
}


inline
void Object::inc_ref_count() const {
    Object& self = *const_cast<Object*>(this);
    switch (m_fields.repr_ix) {
        case EMPTY: throw empty_reference();
        case STR:   ++(std::get<1>(*self.m_repr.ps).m_fields.ref_count); break;
        case LIST:  ++(std::get<1>(*self.m_repr.pl).m_fields.ref_count); break;
        case MAP:   ++(std::get<1>(*self.m_repr.psm).m_fields.ref_count); break;
        case OMAP:  ++(std::get<1>(*self.m_repr.pom).m_fields.ref_count); break;
        case DSRC:  m_repr.ds->inc_ref_count(); break;
        default:      break;
    }
}

inline
void Object::dec_ref_count() const {
    Object& self = *const_cast<Object*>(this);
    switch (m_fields.repr_ix) {
        case STR: {
            auto p_irc = self.m_repr.ps;
            if (--(std::get<1>(*p_irc).m_fields.ref_count) == 0) {
                delete p_irc;
            }
            break;
        }
        case LIST: {
            auto p_irc = self.m_repr.pl;
            if (--(std::get<1>(*p_irc).m_fields.ref_count) == 0) {
                for (auto& value : std::get<0>(*p_irc))
                    value.clear_parent();
                delete p_irc;
            }
            break;
        }
        case MAP: {
            auto p_irc = self.m_repr.psm;
            if (--(std::get<1>(*p_irc).m_fields.ref_count) == 0) {
                for (auto& item : std::get<0>(*p_irc))
                    item.second.clear_parent();
                delete p_irc;
            }
            break;
        }
        case OMAP: {
            auto p_irc = self.m_repr.pom;
            if (--(std::get<1>(*p_irc).m_fields.ref_count) == 0) {
                for (auto& item : std::get<0>(*p_irc))
                    item.second.clear_parent();
                delete p_irc;
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
        case NIL:  m_repr.z = nullptr; break;
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
        case MAP: {
            m_repr.psm = other.m_repr.psm;
            inc_ref_count();
            break;
        }
        case OMAP: {
            m_repr.pom = other.m_repr.pom;
            inc_ref_count();
            break;
        }
        case DSRC: {
            m_repr.ds = other.m_repr.ds;
            inc_ref_count();
            break;
        }
    }
    return *this;
}

inline
Object& Object::operator = (Object&& other) {
    dec_ref_count();

    m_fields.repr_ix = other.m_fields.repr_ix;
    switch (m_fields.repr_ix) {
        case EMPTY: m_repr.z = nullptr; break;
        case NIL:  m_repr.z = nullptr; break;
        case BOOL:  m_repr.b = other.m_repr.b; break;
        case INT:   m_repr.i = other.m_repr.i; break;
        case UINT:  m_repr.u = other.m_repr.u; break;
        case FLOAT: m_repr.f = other.m_repr.f; break;
        case STR:   m_repr.ps = other.m_repr.ps; break;
        case LIST:  m_repr.pl = other.m_repr.pl; break;
        case MAP:   m_repr.psm = other.m_repr.psm; break;
        case OMAP:  m_repr.pom = other.m_repr.pom; break;
        case DSRC:  m_repr.ds = other.m_repr.ds; break;
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


//////////////////////////////////////////////////////////////////////////////
/// @brief An iterator over the ancestors of an Object.
/// The first object returned by this iterator is always the object, itself.
/// So, this iterator is like the XML ancestors-or-self axis.
//////////////////////////////////////////////////////////////////////////////
class LineIterator
{
  public:
    LineIterator(const Object& object) : m_object{object} {}
    LineIterator(Object&& object) : m_object{std::forward<Object>(object)} {}

    LineIterator(const LineIterator&) = default;
    LineIterator(LineIterator&&) = default;

    LineIterator& operator ++ () {
        m_object.refer_to(m_object.parent());
        if (m_object == nil) m_object.release();
        return *this;
    }
    Object operator * () const { return m_object; }
    bool operator == (const LineIterator& other) const {
        // m_object may be empty - see LineRange::end
        if (m_object.is_empty()) return other.m_object.is_empty();
        return !other.m_object.is_empty() && m_object.is(other.m_object);
    }
  private:
    Object m_object;
};


//////////////////////////////////////////////////////////////////////////////
/// @brief A range-like object for LineIterator.
//////////////////////////////////////////////////////////////////////////////
class LineRange
{
  public:
    LineRange(const Object& object) : m_object{object} {}
    LineIterator begin() const { return m_object; }
    LineIterator end() const   { return Object{}; }
  private:
    Object m_object;
};

inline
LineRange Object::iter_line() const {
    return *this;
}


//////////////////////////////////////////////////////////////////////////////
/// @brief An iterator over all descendants of an Object.
/// This iterator is like the XML descendants-or-self axis.
//////////////////////////////////////////////////////////////////////////////
template <class ValueType, typename VisitPred, typename EnterPred>
class TreeIterator
{
  public:
    using TreeRange = Object::TreeRange<ValueType, VisitPred, EnterPred>;

  public:
    TreeIterator() = default;  // python creates, then assigns

    TreeIterator(TreeRange* p_range) : mp_range{p_range} {}

    TreeIterator(TreeRange* p_range, List& list)
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
            if (obj.is_container() && mp_range->should_enter(obj))
                mp_range->fifo().push_back(obj);
        } else {
            if (obj.is_container())
                mp_range->fifo().push_back(obj);
        }
    }

  private:
    TreeRange* mp_range;
    ValueIterator m_it;
    ValueIterator m_end;
};


//////////////////////////////////////////////////////////////////////////////
/// @brief A range-like object for TreeIterator.
//////////////////////////////////////////////////////////////////////////////
template <class ValueType, typename VisitPred, typename EnterPred>
class Object::TreeRange
{
  public:
    using TreeIterator = TreeIterator<ValueType, VisitPred, EnterPred>;

  public:
    TreeRange() = default;  // python creates, then assigns

    TreeRange(const Object& object, VisitPred&& visit_pred, EnterPred&& enter_pred)
      : m_visit_pred{visit_pred}
      , m_enter_pred{enter_pred}
    {
        m_list.push_back(object);
    }

    TreeIterator begin() { return TreeIterator{this, m_list}; }
    TreeIterator end()   { return TreeIterator{this}; }

    bool should_visit(const Object& obj) const { return !m_visit_pred || m_visit_pred(obj); }
    bool should_enter(const Object& obj) const { return !m_enter_pred || m_enter_pred(obj); }
    auto& fifo() { return m_fifo; }

  private:
    List m_list;
    std::deque<ValueRange> m_fifo;
    VisitPred m_visit_pred;
    EnterPred m_enter_pred;
};

inline
Object::TreeRange<Object, NoPredicate, NoPredicate> Object::iter_tree() const {
    return {*this, NoPredicate{}, NoPredicate{}};
}

template <typename VisitPred>
Object::TreeRange<Object, Predicate, NoPredicate> Object::iter_tree(VisitPred&& visit_pred) const {
    return {*this, std::forward<Predicate>(visit_pred), NoPredicate{}};
}

template <typename EnterPred>
Object::TreeRange<Object, NoPredicate, Predicate> Object::iter_tree_if(EnterPred&& enter_pred) const {
    return {*this, NoPredicate{}, std::forward<Predicate>(enter_pred)};
}

template <typename VisitPred, typename EnterPred>
Object::TreeRange<Object, Predicate, Predicate> Object::iter_tree_if(VisitPred&& visit_pred, EnterPred&& enter_pred) const {
    return {*this, std::forward<Predicate>(visit_pred), std::forward<Predicate>(enter_pred)};
}

inline
void Object::save() {
    auto tree_range = iter_tree_if(has_data_source, is_fully_cached);
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
    switch (m_fields.repr_ix) {
        case DSRC: m_repr.ds->reset(); break;
        default:      break;
    }
}

inline
void Object::reset_key(const Key& key) {
    switch (m_fields.repr_ix) {
        case DSRC: m_repr.ds->reset_key(key); break;
        default:      break;
    }
}

inline
void Object::refresh() {
    switch (m_fields.repr_ix) {
        case DSRC: m_repr.ds->refresh(); break;
        default:      break;
    }
}

inline
void Object::refresh_key(const Key& key) {
    switch (m_fields.repr_ix) {
        case DSRC: m_repr.ds->refresh_key(key); break;
        default:      break;
    }
}

inline
std::ostream& operator<< (std::ostream& ostream, const Object& obj) {
    ostream << obj.to_str();
    return ostream;
}

inline
void DataSource::Options::configure(const Object& uri) {
    mode = Mode::READ;
    auto query_mode = uri.get("query.mode"_path);
    if (query_mode != nil) {
        auto mode_s = query_mode.as<String>();
        if (mode_s.find_first_of("rR") != mode_s.npos) mode |= Mode::READ;
        if (mode_s.find_first_of("wW") != mode_s.npos) mode |= Mode::WRITE;
        if (mode_s.find_first_of("cC") != mode_s.npos) mode |= Mode::CLOBBER;
    }
}

/**
 * @brief Bind an object to a DataSource.
 * @param obj The object to be bound.
 * - If the object has a parent, the object is first removed from the parent, bound to the DataSource,
 *   and then added back to the parent.
 * - The object must not already be bound to another DataSource.
 * - This is a low-level method that should not be called directly by users.
 * - The behavior is undefined (probably terrible) if the DataSource is already bound to another object.
 * - A WrongType exception is thrown if the DataSource requires a specific object type, and the object argument
 *   has a different type.
 */
inline
void DataSource::bind(Object& obj) {
    if (m_repr_ix != obj.m_fields.repr_ix)
        throw Object::wrong_type(obj.m_fields.repr_ix);

    Key key;
    Object parent = obj.parent();
    if (parent != nil) {
        key = parent.key_of(obj);
        obj.clear_parent();
    }

    m_cache = obj;
    m_fully_cached = false;
    m_unsaved = true;

    obj = this;

    if (parent != nil) {
        parent.set(key, obj);
    }
}

inline
const Object& DataSource::get_cached(const Object& target) const {
    const_cast<DataSource*>(this)->insure_fully_cached(target); return m_cache;
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
        for (it.next(); !it.done(); it.next()) {
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
void DataSource::set(const Object& target, const Slice& slice, const Object& in_vals) {
    set_unsaved(true);
    if (is_sparse()) {
        if (m_cache.is_empty()) read_type(target);
    } else {
        insure_fully_cached(target);
    }
    m_cache.set(slice, in_vals);
    for (auto val : m_cache.get(slice).iter_values())
        val.set_parent(target);
}

inline
void DataSource::del(const Object& target, const Key& key) {
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
    set_unsaved(true);
    if (is_sparse()) {
        if (m_cache.is_empty()) read_type(target);
        for (auto& obj : m_cache.get(slice).values())
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
void DataSource::save(const Object& target) {
    if (!(resolve_mode() & Mode::WRITE)) throw WriteProtect();
    if (m_cache.m_fields.repr_ix == Object::EMPTY) return;

    if (m_unsaved) {
        m_write_failed = false;

        if (m_fully_cached) {
            write(target, m_cache);
        }
        else if (is_sparse()) {
            KeyList deleted_keys;

            for (auto& [key, value] : m_cache.items()) {
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
            commit(target, deleted_keys);
        }

        if (!m_write_failed)
            m_unsaved = false;
    }
}

inline
void DataSource::reset() {
    m_fully_cached = false;
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
    // TODO: implement refresh
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

        switch (m_cache.m_fields.repr_ix) {
            case Object::LIST: {
                for (auto& value : std::get<0>(*m_cache.m_repr.pl))
                    value.set_parent(target);  // TODO: avoid setting parent twice
                break;
            }
            case Object::MAP: {
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
    m_cache.set(value);
}

inline
void DataSource::read_set(const Object& target, const Key& key, const Object& value) {
    m_cache.set(key, value);
    value.set_parent(target);
}

inline
void DataSource::read_set(const Object& target, Key&& key, const Object& value) {
    m_cache.set(std::forward<Key>(key), value);
    value.set_parent(target);
}

inline
void DataSource::read_del(const Object& target, const Key& key) {
    m_cache.del(key);
}


//////////////////////////////////////////////////////////////////////////////
/// @brief This class implements the Object subscript operator.
//////////////////////////////////////////////////////////////////////////////
template <class T>
class Object::Subscript : private Object
{
  public:
    Subscript(const Object& obj, const T& sub) : Object{obj}, m_sub{sub} {}

    Subscript<Key> operator [] (const Key& key);
    Subscript<OPath> operator [] (const OPath& path);

    Object operator = (const Object& obj) { return Object::set(m_sub, obj); }
    String to_str() const { return Object::get(m_sub).to_str(); }

  private:
    T m_sub;
};

inline
Object::Subscript<Key> Object::operator [] (const Key& key) { return {*this, key}; }

inline
Object::Subscript<OPath> Object::operator [] (const OPath& path) { return {*this, path}; }

template <class T>
Object::Subscript<Key> Object::Subscript<T>::operator [] (const Key& key) { return {get(m_sub), key}; }

template <class T>
Object::Subscript<OPath> Object::Subscript<T>::operator [] (const OPath& path) { return {get(m_sub), path}; }


namespace test {

//////////////////////////////////////////////////////////////////////////////
/// @brief Test interface for examining the private DataSource cache.
//////////////////////////////////////////////////////////////////////////////
class DataSourceTestInterface
{
  public:
    DataSourceTestInterface(DataSource& data_source) : m_data_source{data_source} {}
    Object& cache() { return m_data_source.m_cache; }
  private:
    DataSource& m_data_source;
};

} // test namespace
} // nodel namespace


namespace std {

//////////////////////////////////////////////////////////////////////////////
/// @brief OPath hash function
//////////////////////////////////////////////////////////////////////////////
template<>
struct hash<nodel::OPath>
{
    std::size_t operator () (const nodel::OPath& path) const noexcept {
      return path.hash();
    }
};

} // namespace std
