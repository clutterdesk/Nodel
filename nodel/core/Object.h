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
#include <tsl/ordered_map.h>
#include <unordered_set>
#include <stack>
#include <deque>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <variant>
#include <functional>
#include <cstdint>
#include <ctype.h>
#include <errno.h>
#include <fmt/core.h>
#include <memory>
#include <type_traits>

#include "Oid.h"
#include "Key.h"
#include "Interval.h"

#include <nodel/support/Flags.h>
#include <nodel/support/logging.h>
#include <nodel/support/string.h>
#include <nodel/support/exception.h>
#include <nodel/support/parse.h>
#include <nodel/types.h>


namespace nodel {

struct EmptyReference : public NodelException
{
    EmptyReference() : NodelException("uninitialized object"s) {}
};

struct WriteProtected : public NodelException
{
    WriteProtected() : NodelException("Data-source is write protected"s) {}
};

struct OverwriteProtected : public NodelException
{
    OverwriteProtected() : NodelException("Data-source is over-write protected"s) {}
};

struct InvalidPath : public NodelException
{
    InvalidPath() : NodelException("Invalid object path"s) {}
};


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


// NOTE: use by-value semantics
class Object
{
  public:
    template <class ValueType, typename VisitPred, typename EnterPred> class TreeRange;

    // TODO: remove _I suffix
    enum ReprType {
        EMPTY_I,   // uninitialized
        NULL_I,    // json null, also used for non-existent parent
        BOOL_I,
        INT_I,
        UINT_I,
        FLOAT_I,
        STR_I,
        LIST_I,
        MAP_I,     // sorted map
        OMAP_I,    // ordered map
        DSRC_I,    // DataSource
        DEL_I,     // indicates deleted key in sparse data-store
        BAD_I = 31
    };

  private:
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
    template <typename T> ReprType get_repr_ix() const;
    template <> ReprType get_repr_ix<bool>() const       { return BOOL_I; }
    template <> ReprType get_repr_ix<Int>() const        { return INT_I; }
    template <> ReprType get_repr_ix<UInt>() const       { return UINT_I; }
    template <> ReprType get_repr_ix<Float>() const      { return FLOAT_I; }
    template <> ReprType get_repr_ix<String>() const     { return STR_I; }
    template <> ReprType get_repr_ix<List>() const       { return LIST_I; }
    template <> ReprType get_repr_ix<SortedMap>() const  { return MAP_I; }
    template <> ReprType get_repr_ix<OrderedMap>() const { return OMAP_I; }

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
    Object(NoParent&&)      : m_repr{}, m_fields{NULL_I} {}  // initialize reference count

  public:
    static std::string_view type_name(uint8_t repr_ix) {
      switch (repr_ix) {
          case EMPTY_I: return "empty";
          case NULL_I:  return "null";
          case BOOL_I:  return "bool";
          case INT_I:   return "int";
          case UINT_I:  return "uint";
          case FLOAT_I: return "double";
          case STR_I:   return "string";
          case LIST_I:  return "list";
          case MAP_I:   return "sorted-map";
          case OMAP_I:  return "ordered-map";
          case DSRC_I:  return "data-source";
          case DEL_I:   return "deleted";
          default:      return "<undefined>";
      }
    }

  public:
    static constexpr refcnt_t no_ref_count = std::numeric_limits<refcnt_t>::max();

    Object()                           : m_repr{}, m_fields{EMPTY_I} {}
    Object(null_t)                     : m_repr{}, m_fields{NULL_I} {}
    Object(const std::string& str)     : m_repr{new IRCString{str, NoParent()}}, m_fields{STR_I} {}
    Object(std::string&& str)          : m_repr{new IRCString(std::forward<std::string>(str), NoParent())}, m_fields{STR_I} {}
    Object(const std::string_view& sv) : m_repr{new IRCString{{sv.data(), sv.size()}, NoParent()}}, m_fields{STR_I} {}
    Object(const char* v)              : m_repr{new IRCString{v, NoParent()}}, m_fields{STR_I} { ASSERT(v != nullptr); }
    Object(bool v)                     : m_repr{v}, m_fields{BOOL_I} {}
    Object(is_like_Float auto v)       : m_repr{(Float)v}, m_fields{FLOAT_I} {}
    Object(is_like_Int auto v)         : m_repr{(Int)v}, m_fields{INT_I} {}
    Object(is_like_UInt auto v)        : m_repr{(UInt)v}, m_fields{UINT_I} {}
    Object(DataSourcePtr ptr);

    Object(const List&);
    Object(const SortedMap&);
    Object(const OrderedMap&);
    Object(List&&);
    Object(SortedMap&&);
    Object(OrderedMap&&);

    Object(const Key&);
    Object(Key&&);

    Object(ReprType type);

    Object(const Object& other);
    Object(Object&& other);

    ~Object() { dec_ref_count(); }

    ReprType type() const;
    std::string_view type_name() const { return type_name(type()); }

    Object root() const;
    Object parent() const;

    KeyRange iter_keys() const;
    ItemRange iter_items() const;
    ValueRange iter_values() const;
    LineRange iter_line() const;

    KeyRange iter_keys(const Interval&) const;
    ItemRange iter_items(const Interval&) const;
    ValueRange iter_values(const Interval&) const;

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
        else if (m_fields.repr_ix == DSRC_I) return dsrc_read().as<T>();
        throw wrong_type(m_fields.repr_ix);
    }

    template <typename T>
    const T& as() const requires std::is_same<T, String>::value {
        if (m_fields.repr_ix == get_repr_ix<T>()) return get_repr<T>();
        else if (m_fields.repr_ix == DSRC_I) return dsrc_read().as<T>();
        throw wrong_type(m_fields.repr_ix);
    }

    template <typename T>
    T& as() requires is_byvalue<T> {
        if (m_fields.repr_ix == get_repr_ix<T>()) return get_repr<T>();
        else if (m_fields.repr_ix == DSRC_I) return dsrc_read().as<T>();
        throw wrong_type(m_fields.repr_ix);
    }

    template <typename T>
    T& as() requires std::is_same<T, String>::value {
        if (m_fields.repr_ix == get_repr_ix<T>()) return get_repr<T>();
        else if (m_fields.repr_ix == DSRC_I) return dsrc_read().as<T>();
        throw wrong_type(m_fields.repr_ix);
    }

    bool is_empty() const   { return m_fields.repr_ix == EMPTY_I; }
    bool is_deleted() const { return m_fields.repr_ix == DEL_I; }

    bool is_num() const       { auto type = resolve_repr_ix(); return type == INT_I || type == UINT_I || type == FLOAT_I; }
    bool is_map() const       { auto type = resolve_repr_ix(); return type == MAP_I || type == OMAP_I; }
    bool is_container() const { auto type = resolve_repr_ix(); return type == LIST_I || type == OMAP_I || type == MAP_I; }

    bool is_valid() const;

    bool to_bool() const;
    Int to_int() const;
    UInt to_uint() const;
    Float to_float() const;
    std::string to_str() const;
    Key to_key() const;
    Key into_key();
    std::string to_json() const;
    void to_json(std::ostream&) const;

    Object get(is_integral auto v) const;
    Object get(const Key& key) const;
    Object get(const OPath& path) const;

    Object set(const Object&);
    Object set(const Key&, const Object&);
    Object set(Key&&, const Object&);
    Object set(const OPath&, const Object&);

    void del(const Key& key);
    void del(const OPath& path);
    void del_from_parent();

    bool operator == (const Object&) const;
    bool operator == (null_t) const;
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

    void save(bool quiet = false);
    void reset();
    void reset_key(const Key&);
    void refresh();
    void refresh_key(const Key&);

    static WrongType wrong_type(uint8_t actual)                   { return type_name(actual); };
    static WrongType wrong_type(uint8_t actual, uint8_t expected) { return {type_name(actual), type_name(expected)}; };
    static EmptyReference empty_reference()                       { return {}; }

  protected:
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

  friend class WalkDF;
  friend class WalkBF;

  template <class ValueType, typename VisitPred, typename EnterPred> friend class TreeIterator;

  friend bool has_data_source(const Object&);
};


class OPath
{
  public:
    using Iterator = KeyList::iterator;
    using ConstIterator = KeyList::const_iterator;

    OPath() {}
    OPath(const std::string_view& spec);

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
            if (child == null) return child;
            obj.refer_to(child);
        }
        return obj;
    }

    bool is_leaf(const Object& obj) const {
        Object cursor = obj;
        Object parent = obj.parent();
        for (size_t i = 0; i < m_keys.size() && parent != null; ++i) {
            cursor = parent;
            parent = cursor.parent();
        }
        if (cursor == null) return false;
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
                if (child == null) {
                    child = Object{(*it).is_any_int()? Object::LIST_I: Object::OMAP_I};
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

    std::string to_str() const {
        if (m_keys.size() == 0) return ".";
        std::stringstream ss;
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
OPath::OPath(const std::string_view& spec) {
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
            throw SyntaxError{spec, it - spec.cbegin(), "Expected '.' or '[':"};
        }
    }
}

inline
Key OPath::parse_brace_key(const StringView& spec, StringView::const_iterator& it) {
    auto key_start = it;
    char c = *it;
    if (c == '\'' || c == '"') {
        auto key = parse_quoted(spec, it);
        if (key.size() == 0) throw SyntaxError{spec, key_start - spec.cbegin(), "Expected key:"};
        consume_whitespace(spec, it);
        if (*it != ']') throw SyntaxError{spec, key_start - spec.cbegin(), "Missing closing ']':"};
        ++it;
        return key;
    } else {
        for (; it != spec.cend() && c != ']'; ++it, c = *it);
        if (it == spec.cend()) throw SyntaxError{spec, key_start - spec.cbegin(), "Missing closing ']':"};
        std::string_view key{key_start, it};
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
    throw SyntaxError(spec, spec.size() - 1, "Missing closing quote:");
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
    while (par != null) {
        path.append(par.key_of(obj));
        obj.refer_to(par);
        par.refer_to(obj.parent());
    }
    path.reverse();
    return path;
}

inline
OPath Object::path(const Object& root) const {
    if (root == null) return path();
    OPath path;
    Object obj = *this;
    Object par = parent();
    while (par != null && !obj.is(root)) {
        path.append(par.key_of(obj));
        obj.refer_to(par);
        par.refer_to(obj.parent());
    }
    path.reverse();
    return path;
}


class DataSource
{
  public:
    class KeyIterator
    {
      public:
        virtual ~KeyIterator() {}
        void next() { if (!next_impl()) m_key = null; }
        Key& key() { return m_key; }
        bool done() const { return m_key == null; }

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
        void next() { if (!next_impl()) std::get<0>(m_item) = null; }
        Item& item() { return m_item; }
        bool done() const { return m_item.first == null; }

      protected:
        virtual bool next_impl() = 0;
        Item m_item;
    };

  public:
    using ReprType = Object::ReprType;

    enum class Kind { SPARSE, COMPLETE };  // SPARSE -> big key/value source, COMPLETE -> all keys present once cached
    enum class Origin { SOURCE, MEMORY };  // SOURCE -> created while reading source, MEMORY -> created by program

    struct Mode : public Flags<uint8_t> {
        FLAG8 READ = 1;
        FLAG8 WRITE = 2;
        FLAG8 OVERWRITE = 4;
        FLAG8 ALL = 7;
        FLAG8 INHERIT = 8;
        Mode(Flags<uint8_t> flags) : Flags<uint8_t>(flags) {}
        operator int () const { return m_value; }
    };

    DataSource(Kind kind, Mode mode, Origin origin)
      : m_parent{Object::NULL_I}
      , m_cache{Object::EMPTY_I}
      , m_mode{mode}
      , m_fully_cached{(kind == Kind::COMPLETE) && origin == Origin::MEMORY}
      , m_unsaved{origin == Origin::MEMORY}
      , m_kind{kind}
      , m_default_repr_ix{Object::EMPTY_I}
    {}

    DataSource(Kind kind, Mode mode, ReprType repr_ix, Origin origin)
      : m_parent{Object::NULL_I}
      , m_cache{repr_ix}
      , m_mode{mode}
      , m_fully_cached{(kind == Kind::COMPLETE) && origin == Origin::MEMORY}
      , m_unsaved{origin == Origin::MEMORY}
      , m_kind{kind}
      , m_default_repr_ix{repr_ix}
    {}

    virtual ~DataSource() {}

  protected:
    virtual DataSource* new_instance(const Object& target, Origin origin) const = 0;
    virtual void read_type(const Object& target)                                  {}  // define type/id of object
    virtual void read(const Object& target) = 0;
    virtual Object read_key(const Object& target, const Key&)                      { return {}; }  // sparse
    virtual void write(const Object& target, const Object&, bool)                  {}  // both
    virtual void write_key(const Object& target, const Key&, const Object&, bool)  {}  // sparse
    virtual void delete_key(const Object& target, const Key&, bool)                {}  // sparse
    virtual void commit(const Object& target, const KeyList& del_keys, bool quiet) {} // sparse

    // interface to use from virtual read methods
    void read_set(const Object& target, const Object& value);
    void read_set(const Object& target, const Key& key, const Object& value);
    void read_set(const Object& target, Key&& key, const Object& value);
    void read_del(const Object& target, const Key& key);

    virtual std::string to_str(const Object& target) {
        insure_fully_cached(target);
        return m_cache.to_str();
    }

    virtual std::unique_ptr<KeyIterator> key_iter()     { return nullptr; }
    virtual std::unique_ptr<ValueIterator> value_iter() { return nullptr; }
    virtual std::unique_ptr<ItemIterator> item_iter()   { return nullptr; }

    virtual std::unique_ptr<KeyIterator> key_iter(const Interval&)     { return nullptr; }
    virtual std::unique_ptr<ValueIterator> value_iter(const Interval&) { return nullptr; }
    virtual std::unique_ptr<ItemIterator> item_iter(const Interval&)   { return nullptr; }

  public:
    const Object& get_cached(const Object& target) const { const_cast<DataSource*>(this)->insure_fully_cached(target); return m_cache; }
    Object& get_cached(const Object& target)             { insure_fully_cached(target); return m_cache; }

    size_t size(const Object& target);
    Object get(const Object& target, const Key& key);

    Mode mode() const        { return m_mode; }
    void set_mode(Mode mode) { m_mode = mode; }
    Mode resolve_mode() const;

    bool is_fully_cached() const { return m_fully_cached; }
    bool is_sparse() const { return m_kind == Kind::SPARSE; }

    void set_failed(bool failed) { m_failed = failed; }
    bool is_valid(const Object& target);

  private:
    DataSource* copy(const Object& target, Origin origin) const;
    void set(const Object& target, const Object& in_val);
    Object set(const Object& target, const Key& key, const Object& in_val);
    Object set(const Object& target, Key&& key, const Object& in_val);
    void del(const Object& target, const Key& key);

    void save(const Object& target, bool quiet);

    void reset();
    void reset_key(const Key& key);
    void refresh();
    void refresh_key(const Key& key);

    const Key key_of(const Object& obj) { return m_cache.key_of(obj); }
    ReprType type(const Object& target) { if (m_cache.is_empty()) read_type(target); return m_cache.type(); }
    Oid id(const Object& target)        { if (m_cache.is_empty()) read_type(target); return m_cache.id(); }

    void insure_fully_cached(const Object& target);
    void set_unsaved(bool unsaved) { m_unsaved = unsaved; }

    refcnt_t ref_count() const { return m_parent.m_fields.ref_count; }
    void inc_ref_count()       { m_parent.m_fields.ref_count++; }
    refcnt_t dec_ref_count()   { return --(m_parent.m_fields.ref_count); }

  private:
    Object m_parent;
    Object m_cache;
    Mode m_mode;
    bool m_fully_cached = false;
    bool m_unsaved = false;
    bool m_failed = false;
    Kind m_kind;
    ReprType m_default_repr_ix;

  friend class Object;
  friend class KeyRange;
  friend class ValueRange;
  friend class ItemRange;
  friend class ::nodel::test::DataSourceTestInterface;
};

inline
bool has_data_source(const Object& obj) {
    return obj.m_fields.repr_ix == Object::DSRC_I;
}

inline
bool is_fully_cached(const Object& obj) {
    auto p_ds = obj.data_source<DataSource>();
    return p_ds == nullptr || p_ds->is_fully_cached();
}

inline
Object::Object(const Object& other) : m_fields{other.m_fields} {
    switch (m_fields.repr_ix) {
        case EMPTY_I: m_repr.z = nullptr; break;
        case NULL_I:  m_repr.z = nullptr; break;
        case BOOL_I:  m_repr.b = other.m_repr.b; break;
        case INT_I:   m_repr.i = other.m_repr.i; break;
        case UINT_I:  m_repr.u = other.m_repr.u; break;
        case FLOAT_I: m_repr.f = other.m_repr.f; break;
        case STR_I:   m_repr.ps = other.m_repr.ps; inc_ref_count(); break;
        case LIST_I:  m_repr.pl = other.m_repr.pl; inc_ref_count(); break;
        case MAP_I:   m_repr.psm = other.m_repr.psm; inc_ref_count(); break;
        case OMAP_I:  m_repr.pom = other.m_repr.pom; inc_ref_count(); break;
        case DSRC_I:  m_repr.ds = other.m_repr.ds; m_repr.ds->inc_ref_count(); break;
    }
}

inline
Object::Object(Object&& other) : m_fields{other.m_fields} {
    switch (m_fields.repr_ix) {
        case EMPTY_I: m_repr.z = nullptr; break;
        case NULL_I:  m_repr.z = nullptr; break;
        case BOOL_I:  m_repr.b = other.m_repr.b; break;
        case INT_I:   m_repr.i = other.m_repr.i; break;
        case UINT_I:  m_repr.u = other.m_repr.u; break;
        case FLOAT_I: m_repr.f = other.m_repr.f; break;
        case STR_I:   m_repr.ps = other.m_repr.ps; break;
        case LIST_I:  m_repr.pl = other.m_repr.pl; break;
        case MAP_I:   m_repr.psm = other.m_repr.psm; break;
        case OMAP_I:  m_repr.pom = other.m_repr.pom; break;
        case DSRC_I:  m_repr.ds = other.m_repr.ds; break;
    }

    other.m_fields.repr_ix = EMPTY_I;
    other.m_repr.z = nullptr;
}

inline
Object::Object(DataSourcePtr ptr) : m_repr{ptr}, m_fields{DSRC_I} {}  // DataSource ownership transferred

inline
Object::Object(const List& list) : m_repr{new IRCList({}, NoParent{})}, m_fields{LIST_I} {
    auto& my_list = std::get<0>(*m_repr.pl);
    for (auto& value : list) {
        Object copy = value.copy();
        my_list.push_back(copy);
        copy.set_parent(*this);
    }
}

inline
Object::Object(const SortedMap& map) : m_repr{new IRCMap({}, NoParent{})}, m_fields{MAP_I} {
    auto& my_map = std::get<0>(*m_repr.psm);
    for (auto& [key, value]: map) {
        Object copy = value.copy();
        my_map.insert({key, copy});
        copy.set_parent(*this);
    }
}

inline
Object::Object(const OrderedMap& map) : m_repr{new IRCOMap({}, NoParent{})}, m_fields{OMAP_I} {
    auto& my_map = std::get<0>(*m_repr.pom);
    for (auto& [key, value]: map) {
        Object copy = value.copy();
        my_map.insert({key, copy});
        copy.set_parent(*this);
    }
}

inline
Object::Object(List&& list) : m_repr{new IRCList(std::forward<List>(list), NoParent{})}, m_fields{LIST_I} {
    auto& my_list = std::get<0>(*m_repr.pl);
    for (auto& obj : my_list)
        obj.set_parent(*this);
}

inline
Object::Object(SortedMap&& map) : m_repr{new IRCMap(std::forward<SortedMap>(map), NoParent{})}, m_fields{MAP_I} {
    auto& my_map = std::get<0>(*m_repr.psm);
    for (auto& [key, obj]: my_map)
        const_cast<Object&>(obj).set_parent(*this);
}

inline
Object::Object(OrderedMap&& map) : m_repr{new IRCOMap(std::forward<OrderedMap>(map), NoParent{})}, m_fields{OMAP_I} {
    auto& my_map = std::get<0>(*m_repr.pom);
    for (auto& [key, obj]: my_map)
        const_cast<Object&>(obj).set_parent(*this);
}

inline
Object::Object(const Key& key) : m_fields{EMPTY_I} {
    switch (key.m_repr_ix) {
        case Key::NULL_I:  m_fields.repr_ix = NULL_I; m_repr.z = nullptr; break;
        case Key::BOOL_I:  m_fields.repr_ix = BOOL_I; m_repr.b = key.m_repr.b; break;
        case Key::INT_I:   m_fields.repr_ix = INT_I; m_repr.i = key.m_repr.i; break;
        case Key::UINT_I:  m_fields.repr_ix = UINT_I; m_repr.u = key.m_repr.u; break;
        case Key::FLOAT_I: m_fields.repr_ix = FLOAT_I; m_repr.f = key.m_repr.f; break;
        case Key::STR_I: {
            m_fields.repr_ix = STR_I;
            m_repr.ps = new IRCString{String{key.m_repr.s.data()}, {}};
            break;
        }
        default: throw std::invalid_argument("key");
    }
}

inline
Object::Object(Key&& key) : m_fields{EMPTY_I} {
    switch (key.m_repr_ix) {
        case Key::NULL_I:  m_fields.repr_ix = NULL_I; m_repr.z = nullptr; break;
        case Key::BOOL_I:  m_fields.repr_ix = BOOL_I; m_repr.b = key.m_repr.b; break;
        case Key::INT_I:   m_fields.repr_ix = INT_I; m_repr.i = key.m_repr.i; break;
        case Key::UINT_I:  m_fields.repr_ix = UINT_I; m_repr.u = key.m_repr.u; break;
        case Key::FLOAT_I: m_fields.repr_ix = FLOAT_I; m_repr.f = key.m_repr.f; break;
        case Key::STR_I: {
            m_fields.repr_ix = STR_I;
            m_repr.ps = new IRCString{String{key.m_repr.s.data()}, {}};
            break;
        }
        default: throw std::invalid_argument("key");
    }
}

inline
Object::Object(ReprType type) : m_fields{(uint8_t)type} {
    switch (type) {
        case EMPTY_I: m_repr.z = nullptr; break;
        case NULL_I:  m_repr.z = nullptr; break;
        case BOOL_I:  m_repr.b = false; break;
        case INT_I:   m_repr.i = 0; break;
        case UINT_I:  m_repr.u = 0; break;
        case FLOAT_I: m_repr.f = 0.0; break;
        case STR_I:   m_repr.ps = new IRCString{"", NoParent{}}; break;
        case LIST_I:  m_repr.pl = new IRCList{{}, NoParent{}}; break;
        case MAP_I:   m_repr.psm = new IRCMap{{}, NoParent{}}; break;
        case OMAP_I:  m_repr.pom = new IRCOMap{{}, NoParent{}}; break;
        case DEL_I:   m_repr.z = nullptr; break;
        default:      throw wrong_type(type);
    }
}

inline
Object::ReprType Object::type() const {
    switch (m_fields.repr_ix) {
        case DSRC_I: return m_repr.ds->type(*this);
        default:     return (ReprType)m_fields.repr_ix;
    }
}

inline
Object Object::root() const {
    Object obj = *this;
    Object par = parent();
    while (par != null) {
        obj.refer_to(par);
        par.refer_to(par.parent());
    }
    return obj;
}

inline
Object Object::parent() const {
    switch (m_fields.repr_ix) {
        case EMPTY_I: throw empty_reference();
        case STR_I:   return std::get<1>(*m_repr.ps);
        case LIST_I:  return std::get<1>(*m_repr.pl);
        case MAP_I:   return std::get<1>(*m_repr.psm);
        case OMAP_I:  return std::get<1>(*m_repr.pom);
        case DSRC_I:  return m_repr.ds->m_parent;
        default:      return null;
    }
}

template <typename V>
void Object::visit(V&& visitor) const {
    switch (m_fields.repr_ix) {
        case EMPTY_I: throw empty_reference();
        case NULL_I:  visitor(nullptr); break;
        case BOOL_I:  visitor(m_repr.b); break;
        case INT_I:   visitor(m_repr.i); break;
        case UINT_I:  visitor(m_repr.u); break;
        case FLOAT_I: visitor(m_repr.f); break;
        case STR_I:   visitor(std::get<0>(*m_repr.ps)); break;
        case DSRC_I:  visitor(*this); break;  // TODO: How to visit Object with DataSource?
        default:      throw wrong_type(m_fields.repr_ix);
    }
}

inline
int Object::resolve_repr_ix() const {
    switch (m_fields.repr_ix) {
        case DSRC_I: return const_cast<DataSourcePtr>(m_repr.ds)->type(*this);
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
        case EMPTY_I: throw empty_reference();
        case NULL_I:  m_repr.z = nullptr; break;
//        case BOOL_I:  m_repr.b = other.m_repr.b; break;
//        case INT_I:   m_repr.i = other.m_repr.i; break;
//        case UINT_I:  m_repr.u = other.m_repr.u; break;
//        case FLOAT_I: m_repr.f = other.m_repr.f; break;
//        case STR_I:   m_repr.ps = other.m_repr.ps; break;
        case LIST_I:  m_repr.pl = other.m_repr.pl; break;
        case MAP_I:   m_repr.psm = other.m_repr.psm; break;
        case OMAP_I:  m_repr.pom = other.m_repr.pom; break;
        case DSRC_I:  m_repr.ds = other.m_repr.ds; break;
        default:      throw wrong_type(m_fields.repr_ix);
    }
}

inline
void Object::set_parent(const Object& new_parent) const {
    switch (m_fields.repr_ix) {
        case STR_I: {
            auto& parent = std::get<1>(*m_repr.ps);
            parent.weak_refer_to(new_parent);
            break;
        }
        case LIST_I: {
            auto& parent = std::get<1>(*m_repr.pl);
            parent.weak_refer_to(new_parent);
            break;
        }
        case MAP_I: {
            auto& parent = std::get<1>(*m_repr.psm);
            parent.weak_refer_to(new_parent);
            break;
        }
        case OMAP_I: {
            auto& parent = std::get<1>(*m_repr.pom);
            parent.weak_refer_to(new_parent);
            break;
        }
        case DSRC_I: {
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
        case STR_I: {
            auto& parent = std::get<1>(*m_repr.ps);
            parent.m_fields.repr_ix = NULL_I;
            break;
        }
        case LIST_I: {
            auto& parent = std::get<1>(*m_repr.pl);
            parent.m_fields.repr_ix = NULL_I;
            break;
        }
        case MAP_I: {
            auto& parent = std::get<1>(*m_repr.psm);
            parent.m_fields.repr_ix = NULL_I;
            break;
        }
        case OMAP_I: {
            auto& parent = std::get<1>(*m_repr.pom);
            parent.m_fields.repr_ix = NULL_I;
            break;
        }
        case DSRC_I: {
            m_repr.ds->m_parent.m_fields.repr_ix = NULL_I;
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
        case NULL_I: break;
        case LIST_I: {
            const auto& list = std::get<0>(*m_repr.pl);
            UInt index = 0;
            for (const auto& item : list) {
                if (item.is(obj))
                    return index;
                index++;
            }
            break;
        }
        case MAP_I: {
            const auto& map = std::get<0>(*m_repr.psm);
            for (auto& [key, value] : map) {
                if (value.is(obj))
                    return key;
            }
            break;
        }
        case OMAP_I: {
            const auto& map = std::get<0>(*m_repr.pom);
            for (auto& [key, value] : map) {
                if (value.is(obj))
                    return key;
            }
            break;
        }
        case DSRC_I: {
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
        case BOOL_I:  return (T)m_repr.b;
        case INT_I:   return (T)m_repr.i;
        case UINT_I:  return (T)m_repr.u;
        case FLOAT_I: return (T)m_repr.f;
        case DSRC_I:  return m_repr.ds->get_cached(*this).value_cast<T>();
        default:      throw wrong_type(m_fields.repr_ix);
    }
}

inline
std::string Object::to_str() const {
    switch (m_fields.repr_ix) {
        case EMPTY_I: throw empty_reference();
        case NULL_I:  return "null";
        case BOOL_I:  return m_repr.b? "true": "false";
        case INT_I:   return int_to_str(m_repr.i);
        case UINT_I:  return int_to_str(m_repr.u);
        case FLOAT_I: return nodel::float_to_str(m_repr.f);
        case STR_I:   return std::get<0>(*m_repr.ps);
        case LIST_I:  [[fallthrough]];
        case MAP_I:   [[fallthrough]];
        case OMAP_I:  return to_json();
        case DSRC_I:  return const_cast<DataSourcePtr>(m_repr.ds)->to_str(*this);
        default:
            throw wrong_type(m_fields.repr_ix);
    }
}

inline
bool Object::is_valid() const {
    if (m_fields.repr_ix == DSRC_I)
        return m_repr.ds->is_valid(*this);
    return true;
}

inline
bool Object::to_bool() const {
    switch (m_fields.repr_ix) {
        case EMPTY_I: throw empty_reference();
        case NULL_I:  throw wrong_type(m_fields.repr_ix, BOOL_I);
        case BOOL_I:  return m_repr.b;
        case INT_I:   return m_repr.i;
        case UINT_I:  return m_repr.u;
        case FLOAT_I: return m_repr.f;
        case STR_I:   return str_to_bool(std::get<0>(*m_repr.ps));
        case DSRC_I:  return const_cast<DataSourcePtr>(m_repr.ds)->get_cached(*this).to_bool();
        default:      throw wrong_type(m_fields.repr_ix, BOOL_I);
    }
}

inline
Int Object::to_int() const {
    switch (m_fields.repr_ix) {
        case EMPTY_I: throw empty_reference();
        case NULL_I:  throw wrong_type(m_fields.repr_ix, INT_I);
        case BOOL_I:  return m_repr.b;
        case INT_I:   return m_repr.i;
        case UINT_I:  return m_repr.u;
        case FLOAT_I: return m_repr.f;
        case STR_I:   return str_to_int(std::get<0>(*m_repr.ps));
        case DSRC_I:  return const_cast<DataSourcePtr>(m_repr.ds)->get_cached(*this).to_int();
        default:      throw wrong_type(m_fields.repr_ix, INT_I);
    }
}

inline
UInt Object::to_uint() const {
    switch (m_fields.repr_ix) {
        case EMPTY_I: throw empty_reference();
        case NULL_I:  throw wrong_type(m_fields.repr_ix, UINT_I);
        case BOOL_I:  return m_repr.b;
        case INT_I:   return m_repr.i;
        case UINT_I:  return m_repr.u;
        case FLOAT_I: {
            UInt num = (UInt)m_repr.f;
            return num;
        }
        case STR_I:   return str_to_int(std::get<0>(*m_repr.ps));
        case DSRC_I:  return const_cast<DataSourcePtr>(m_repr.ds)->get_cached(*this).to_uint();
        default:      throw wrong_type(m_fields.repr_ix, UINT_I);
    }
}

inline
Float Object::to_float() const {
    switch (m_fields.repr_ix) {
        case EMPTY_I: throw empty_reference();
        case NULL_I:  throw wrong_type(m_fields.repr_ix, BOOL_I);
        case BOOL_I:  return m_repr.b;
        case INT_I:   return m_repr.i;
        case UINT_I:  return m_repr.u;
        case FLOAT_I: return m_repr.f;
        case STR_I:   return str_to_float(std::get<0>(*m_repr.ps));
        case DSRC_I:  return const_cast<DataSourcePtr>(m_repr.ds)->get_cached(*this).to_float();
        default:      throw wrong_type(m_fields.repr_ix, FLOAT_I);
    }
}

inline
Key Object::to_key() const {
    switch (m_fields.repr_ix) {
        case EMPTY_I: throw empty_reference();
        case NULL_I:  return null;
        case BOOL_I:  return m_repr.b;
        case INT_I:   return m_repr.i;
        case UINT_I:  return m_repr.u;
        case FLOAT_I: return m_repr.f;
        case STR_I:   return std::get<0>(*m_repr.ps);
        case DSRC_I:  return const_cast<DataSourcePtr>(m_repr.ds)->get_cached(*this).to_key();
        default:      throw wrong_type(m_fields.repr_ix);
    }
}

inline
Key Object::into_key() {
    switch (m_fields.repr_ix) {
        case EMPTY_I: throw empty_reference();
        case NULL_I:  return null;
        case BOOL_I:  return m_repr.b;
        case INT_I:   return m_repr.i;
        case UINT_I:  return m_repr.u;
        case FLOAT_I: return m_repr.f;
        case STR_I:   {
            Key k{std::move(std::get<0>(*m_repr.ps))};
            release();
            return k;
        }
        case DSRC_I: {
            Key k{m_repr.ds->get_cached(*this).into_key()};
            release();
            return k;
        }
        default:
            throw wrong_type(m_fields.repr_ix);
    }
}

inline
Object Object::get(is_integral auto index) const {
    switch (m_fields.repr_ix) {
        case EMPTY_I: throw empty_reference();
        case LIST_I: {
            auto& list = std::get<0>(*m_repr.pl);
            if (index < 0) index += list.size();
            if (index < 0 || index >= list.size()) return null;
            return list[index];
        }
        case DSRC_I:  return m_repr.ds->get(*this, index);
        default:      return get(Key{index});
    }
}

inline
Object Object::get(const Key& key) const {
    switch (m_fields.repr_ix) {
        case EMPTY_I: throw empty_reference();
        case LIST_I: {
            if (!key.is_any_int()) return null;
            auto& list = std::get<0>(*m_repr.pl);
            Int index = key.to_int();
            if (index < 0) index += list.size();
            if (index < 0 || index >= list.size()) return null;
            return list[index];
        }
        case MAP_I: {
            auto& map = std::get<0>(*m_repr.psm);
            auto it = map.find(key);
            if (it == map.end()) return null;
            return it->second;
        }
        case OMAP_I: {
            auto& map = std::get<0>(*m_repr.pom);
            auto it = map.find(key);
            if (it == map.end()) return null;
            return it->second;
        }
        case DSRC_I: return m_repr.ds->get(*this, key); break;
        default:     return null;
    }
}

inline
Object Object::get(const OPath& path) const {
    return path.lookup(*this);
}

inline
Object Object::set(const Object& value) {
    auto repr_ix = m_fields.repr_ix;
    if (repr_ix == EMPTY_I) {
        this->operator = (value);
        return value;
    } else {
        auto par = parent();
        if (par == null) {
            if (repr_ix == DSRC_I) {
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
Object Object::set(const Key& key, const Object& in_val) {
    switch (m_fields.repr_ix) {
        case EMPTY_I: throw empty_reference();
        case LIST_I: {
            Object out_val = (in_val.parent() == null)? in_val: in_val.copy();
            auto& list = std::get<0>(*m_repr.pl);
            Int index = key.to_int();
            auto size = list.size();
            if (index < 0) index += size;
            if (index >= size) {
                list.push_back(out_val);
            } else {
                auto it = list.begin() + index;
                it->set_parent(null);
                *it = out_val;
            }
            out_val.set_parent(*this);
            return out_val;
        }
        case MAP_I: {
            Object out_val = (in_val.parent() == null)? in_val: in_val.copy();
            auto& map = std::get<0>(*m_repr.psm);
            auto it = map.find(key);
            if (it != map.end()) {
                it->second.set_parent(null);
                map.erase(it);
            }
            map.insert({key, out_val});
            out_val.set_parent(*this);
            return out_val;
        }
        case OMAP_I: {
            Object out_val = (in_val.parent() == null)? in_val: in_val.copy();
            auto& map = std::get<0>(*m_repr.pom);
            auto it = map.find(key);
            if (it != map.end()) {
                it->second.set_parent(null);
                map.erase(it);
            }
            map.insert({key, out_val});
            out_val.set_parent(*this);
            return out_val;
        }
        case DSRC_I: return m_repr.ds->set(*this, key, in_val);
        default:     throw wrong_type(m_fields.repr_ix);
    }
}

inline
Object Object::set(Key&& key, const Object& in_val) {
    switch (m_fields.repr_ix) {
        case EMPTY_I: throw empty_reference();
        case LIST_I: {
            Object out_val = (in_val.parent() == null)? in_val: in_val.copy();
            auto& list = std::get<0>(*m_repr.pl);
            Int index = key.to_int();
            auto size = list.size();
            if (index < 0) index += size;
            if (index >= size) {
                list.push_back(out_val);
            } else {
                auto it = list.begin() + index;
                it->set_parent(null);
                *it = out_val;
            }
            out_val.set_parent(*this);
            return out_val;
        }
        case MAP_I: {
            Object out_val = (in_val.parent() == null)? in_val: in_val.copy();
            auto& map = std::get<0>(*m_repr.psm);
            auto it = map.find(key);
            if (it != map.end()) {
                it->second.set_parent(null);
                map.erase(it);
            }
            map.insert({std::forward<Key>(key), out_val});
            out_val.set_parent(*this);
            return out_val;
        }
        case OMAP_I: {
            Object out_val = (in_val.parent() == null)? in_val: in_val.copy();
            auto& map = std::get<0>(*m_repr.pom);
            auto it = map.find(key);
            if (it != map.end()) {
                it->second.set_parent(null);
                map.erase(it);
            }
            map.insert({std::forward<Key>(key), out_val});
            out_val.set_parent(*this);
            return out_val;
        }
        case DSRC_I: return m_repr.ds->set(*this, std::forward<Key>(key), in_val);
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
        case EMPTY_I: throw empty_reference();
        case LIST_I: {
            auto& list = std::get<0>(*m_repr.pl);
            Int index = key.to_int();
            auto size = list.size();
            if (index < 0) index += size;
            if (index >= 0) {
                if (index > size) index = size;
                auto it = list.begin() + index;
                Object value = *it;
                list.erase(it);
                value.clear_parent();
            }
            break;
        }
        case MAP_I: {
            auto& map = std::get<0>(*m_repr.psm);
            auto it = map.find(key);
            if (it != map.end()) {
                Object value = it->second;
                value.clear_parent();
                map.erase(it);
            }
            break;
        }
        case OMAP_I: {
            auto& map = std::get<0>(*m_repr.pom);
            auto it = map.find(key);
            if (it != map.end()) {
                Object value = it->second;
                value.clear_parent();
                map.erase(it);
            }
            break;
        }
        case DSRC_I: m_repr.ds->del(*this, key); break;
        default:     throw wrong_type(m_fields.repr_ix);
    }
}

inline
void Object::del(const OPath& path) {
    auto obj = path.lookup(*this);
    if (obj != null) {
        auto par = obj.parent();
        par.del(par.key_of(obj));
    }
}

inline
void Object::del_from_parent() {
    Object par = parent();
    if (par != null)
        par.del(par.key_of(*this));
}

inline
size_t Object::size() const {
    switch (m_fields.repr_ix) {
        case EMPTY_I: throw empty_reference();
        case STR_I:   return std::get<0>(*m_repr.ps).size();
        case LIST_I:  return std::get<0>(*m_repr.pl).size();
        case MAP_I:   return std::get<0>(*m_repr.psm).size();
        case OMAP_I:  return std::get<0>(*m_repr.pom).size();
        case DSRC_I:  return m_repr.ds->size(*this);
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

inline KeyRange Object::iter_keys(const Interval& itvl) const {
    return {*this, itvl};
}

inline ItemRange Object::iter_items(const Interval& itvl) const {
    return {*this, itvl};
}

inline ValueRange Object::iter_values(const Interval& itvl) const {
    return {*this, itvl};
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
        case LIST_I:  {
            for (auto& child : std::get<0>(*m_repr.pl))
                children.push_back(child);
            break;
        }
        case MAP_I:  {
            for (auto& item : std::get<0>(*m_repr.psm))
                children.push_back(item.second);
            break;
        }
        case OMAP_I:  {
            for (auto& item : std::get<0>(*m_repr.pom))
                children.push_back(item.second);
            break;
        }
        case DSRC_I: {
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
        case MAP_I:  {
            for (auto& item : std::get<0>(*m_repr.psm))
                items.push_back(item);
            break;
        }
        case OMAP_I:  {
            for (auto& item : std::get<0>(*m_repr.pom))
                items.push_back(item);
            break;
        }
        case DSRC_I: {
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
        case NULL_I: {
            if (obj.m_fields.repr_ix == NULL_I) return true;
        }
        case BOOL_I: {
            switch (obj.m_fields.repr_ix)
            {
                case BOOL_I:  return m_repr.b == obj.m_repr.b;
                case INT_I:   return m_repr.b == obj.m_repr.i;
                case UINT_I:  return m_repr.b == obj.m_repr.u;
                case FLOAT_I: return m_repr.b == obj.m_repr.f;
                default:
                    throw wrong_type(m_fields.repr_ix);
            }
        }
        case INT_I: {
            switch (obj.m_fields.repr_ix)
            {
                case BOOL_I:  return m_repr.i == obj.m_repr.b;
                case INT_I:   return m_repr.i == obj.m_repr.i;
                case UINT_I:  return m_repr.i == obj.m_repr.u;
                case FLOAT_I: return m_repr.i == obj.m_repr.f;
                default:
                    throw wrong_type(m_fields.repr_ix);
            }
        }
        case UINT_I: {
            switch (obj.m_fields.repr_ix)
            {
                case BOOL_I:  return m_repr.u == obj.m_repr.b;
                case INT_I:   return m_repr.u == obj.m_repr.i;
                case UINT_I:  return m_repr.u == obj.m_repr.u;
                case FLOAT_I: return m_repr.u == obj.m_repr.f;
                default:
                    throw wrong_type(m_fields.repr_ix);
            }
        }
        case FLOAT_I: {
            switch (obj.m_fields.repr_ix)
            {
                case BOOL_I:  return m_repr.f == obj.m_repr.b;
                case INT_I:   return m_repr.f == obj.m_repr.i;
                case UINT_I:  return m_repr.f == obj.m_repr.u;
                case FLOAT_I: return m_repr.f == obj.m_repr.f;
                default:
                    throw wrong_type(m_fields.repr_ix);
            }
        }
        case STR_I: {
            if (obj.m_fields.repr_ix == STR_I) return std::get<0>(*m_repr.ps) == std::get<0>(*obj.m_repr.ps);
            throw wrong_type(m_fields.repr_ix);
        }
        case DSRC_I: return m_repr.ds->get_cached(*this) == obj;
        default:     throw wrong_type(m_fields.repr_ix);
    }
}

inline
bool Object::operator == (null_t) const {
    return m_fields.repr_ix == NULL_I;
}

inline
std::partial_ordering Object::operator <=> (const Object& obj) const {
    using ordering = std::partial_ordering;

    if (is_empty() || obj.is_empty()) throw empty_reference();
    if (is(obj)) return ordering::equivalent;

    switch (m_fields.repr_ix) {
        case NULL_I: {
            if (obj.m_fields.repr_ix != NULL_I)
                throw wrong_type(m_fields.repr_ix);
            return ordering::equivalent;
        }
        case BOOL_I: {
            if (obj.m_fields.repr_ix != BOOL_I)
                throw wrong_type(m_fields.repr_ix);
            return m_repr.b <=> obj.m_repr.b;
        }
        case INT_I: {
            switch (obj.m_fields.repr_ix) {
                case INT_I:   return m_repr.i <=> obj.m_repr.i;
                case UINT_I:
                    if (obj.m_repr.u > std::numeric_limits<Int>::max()) return ordering::less;
                    return m_repr.i <=> (Int)obj.m_repr.u;
                case FLOAT_I: return m_repr.i <=> obj.m_repr.f;
                default:
                    throw wrong_type(m_fields.repr_ix);
            }
            break;
        }
        case UINT_I: {
            switch (obj.m_fields.repr_ix) {
                case INT_I:
                    if (m_repr.u > std::numeric_limits<Int>::max()) return ordering::greater;
                    return (Int)m_repr.u <=> obj.m_repr.i;
                case UINT_I:  return m_repr.u <=> obj.m_repr.u;
                case FLOAT_I: return m_repr.u <=> obj.m_repr.f;
                default:
                    throw wrong_type(m_fields.repr_ix);
            }
        }
        case FLOAT_I: {
            switch (obj.m_fields.repr_ix) {
                case INT_I:   return m_repr.f <=> obj.m_repr.i;
                case UINT_I:  return m_repr.f <=> obj.m_repr.u;
                case FLOAT_I: return m_repr.f <=> obj.m_repr.f;
                default:
                    throw wrong_type(m_fields.repr_ix);
            }
            break;
        }
        case STR_I: {
            if (obj.m_fields.repr_ix != STR_I)
                throw wrong_type(m_fields.repr_ix);
            return std::get<0>(*m_repr.ps) <=> std::get<0>(*obj.m_repr.ps);
        }
        case DSRC_I: {
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
                    case Object::LIST_I: {
                        m_visitor(parent, key, object, event | BEGIN_PARENT);
                        m_stack.emplace(parent, key, object, event | END_PARENT);
                        auto& list = std::get<0>(*object.m_repr.pl);
                        Int index = list.size() - 1;
                        for (auto it = list.crbegin(); it != list.crend(); it++, index--) {
                            m_stack.emplace(object, index, *it, (index == 0)? FIRST_VALUE: NEXT_VALUE);
                        }
                        break;
                    }
                    case Object::MAP_I: {
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
                    case Object::OMAP_I: {
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
                    case Object::DSRC_I: {
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
                case Object::LIST_I: {
                    auto& list = std::get<0>(*object.m_repr.pl);
                    Int index = 0;
                    for (auto it = list.cbegin(); it != list.cend(); it++, index++) {
                        const auto& child = *it;
                        m_deque.emplace_back(object, index, child);
                    }
                    break;
                }
                case Object::MAP_I: {
                    auto& map = std::get<0>(*object.m_repr.psm);
                    for (auto it = map.cbegin(); it != map.cend(); it++) {
                        auto& [key, child] = *it;
                        m_deque.emplace_back(object, key, child);
                    }
                    break;
                }
                case Object::OMAP_I: {
                    auto& map = std::get<0>(*object.m_repr.pom);
                    for (auto it = map.cbegin(); it != map.cend(); it++) {
                        auto& [key, child] = *it;
                        m_deque.emplace_back(object, key, child);
                    }
                    break;
                }
                case Object::DSRC_I: {
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
std::string Object::to_json() const {
    std::stringstream ss;
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
            case EMPTY_I: throw empty_reference();
            case NULL_I:  os << "null"; break;
            case BOOL_I:  os << (object.m_repr.b? "true": "false"); break;
            case INT_I:   os << int_to_str(object.m_repr.i); break;
            case UINT_I:  os << int_to_str(object.m_repr.u); break;
            case FLOAT_I: os << float_to_str(object.m_repr.f); break;
            case STR_I:   os << std::quoted(std::get<0>(*object.m_repr.ps)); break;
            case LIST_I:  os << ((event & WalkDF::BEGIN_PARENT)? '[': ']'); break;
            case MAP_I:   [[fallthrough]];
            case OMAP_I:  os << ((event & WalkDF::BEGIN_PARENT)? '{': '}'); break;
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
        case EMPTY_I: throw empty_reference();
        case NULL_I:  return Oid::null();
        case BOOL_I:  return Oid{repr_ix, m_repr.b};
        case INT_I:   return Oid{repr_ix, *((uint64_t*)(&m_repr.i))};
        case UINT_I:  return Oid{repr_ix, m_repr.u};;
        case FLOAT_I: return Oid{repr_ix, *((uint64_t*)(&m_repr.f))};;
        case STR_I:   return Oid{repr_ix, (uint64_t)(&(std::get<0>(*m_repr.ps)))};
        case LIST_I:  return Oid{repr_ix, (uint64_t)(&(std::get<0>(*m_repr.pl)))};
        case MAP_I:   return Oid{repr_ix, (uint64_t)(&(std::get<0>(*m_repr.pom)))};
        case OMAP_I:  return Oid{repr_ix, (uint64_t)(&(std::get<0>(*m_repr.pom)))};
        case DSRC_I:  return Oid{repr_ix, (uint64_t)(m_repr.ds)};
        default:      throw wrong_type(m_fields.repr_ix);
    }
}

inline
bool Object::is(const Object& other) const {
    auto repr_ix = m_fields.repr_ix;
    if (other.m_fields.repr_ix != repr_ix) return false;
    switch (repr_ix) {
        case EMPTY_I: throw empty_reference();
        case NULL_I:  return true;
        case BOOL_I:  return m_repr.b == other.m_repr.b;
        case INT_I:   return m_repr.i == other.m_repr.i;
        case UINT_I:  return m_repr.u == other.m_repr.u;
        case FLOAT_I: return m_repr.f == other.m_repr.f;
        case STR_I:   return m_repr.ps == other.m_repr.ps;
        case LIST_I:  return m_repr.pl == other.m_repr.pl;
        case MAP_I:   return m_repr.psm == other.m_repr.psm;
        case OMAP_I:  return m_repr.pom == other.m_repr.pom;
        case DSRC_I:  return m_repr.ds == other.m_repr.ds;
        default:      throw wrong_type(m_fields.repr_ix);
    }
}

inline
Object Object::copy() const {
    switch (m_fields.repr_ix) {
        case EMPTY_I: throw empty_reference();
        case NULL_I:  return NULL_I;
        case BOOL_I:  return m_repr.b;
        case INT_I:   return m_repr.i;
        case UINT_I:  return m_repr.u;
        case FLOAT_I: return m_repr.f;
        case STR_I:   return std::get<0>(*m_repr.ps);
        case LIST_I:  return std::get<0>(*m_repr.pl);  // TODO: long-hand deep-copy, instead of using stack
        case MAP_I:   return std::get<0>(*m_repr.psm);  // TODO: long-hand deep-copy, instead of using stack
        case OMAP_I:  return std::get<0>(*m_repr.pom);  // TODO: long-hand deep-copy, instead of using stack
        case DSRC_I:  return m_repr.ds->copy(*this, DataSource::Origin::MEMORY);
        case DEL_I:   return DEL_I;
        default:      throw wrong_type(m_fields.repr_ix);
    }
}

inline
refcnt_t Object::ref_count() const {
    switch (m_fields.repr_ix) {
        case STR_I:   return std::get<1>(*m_repr.ps).m_fields.ref_count;
        case LIST_I:  return std::get<1>(*m_repr.pl).m_fields.ref_count;
        case MAP_I:   return std::get<1>(*m_repr.psm).m_fields.ref_count;
        case OMAP_I:  return std::get<1>(*m_repr.pom).m_fields.ref_count;
        case DSRC_I:  return m_repr.ds->ref_count();
        default:      return no_ref_count;
    }
}


inline
void Object::inc_ref_count() const {
    Object& self = *const_cast<Object*>(this);
    switch (m_fields.repr_ix) {
        case EMPTY_I: throw empty_reference();
        case STR_I:   ++(std::get<1>(*self.m_repr.ps).m_fields.ref_count); break;
        case LIST_I:  ++(std::get<1>(*self.m_repr.pl).m_fields.ref_count); break;
        case MAP_I:   ++(std::get<1>(*self.m_repr.psm).m_fields.ref_count); break;
        case OMAP_I:  ++(std::get<1>(*self.m_repr.pom).m_fields.ref_count); break;
        case DSRC_I:  m_repr.ds->inc_ref_count(); break;
        default:      break;
    }
}

inline
void Object::dec_ref_count() const {
    Object& self = *const_cast<Object*>(this);
    switch (m_fields.repr_ix) {
        case STR_I: {
            auto p_irc = self.m_repr.ps;
            if (--(std::get<1>(*p_irc).m_fields.ref_count) == 0) {
                delete p_irc;
            }
            break;
        }
        case LIST_I: {
            auto p_irc = self.m_repr.pl;
            if (--(std::get<1>(*p_irc).m_fields.ref_count) == 0) {
                for (auto& value : std::get<0>(*p_irc))
                    value.clear_parent();
                delete p_irc;
            }
            break;
        }
        case MAP_I: {
            auto p_irc = self.m_repr.psm;
            if (--(std::get<1>(*p_irc).m_fields.ref_count) == 0) {
                for (auto& item : std::get<0>(*p_irc))
                    item.second.clear_parent();
                delete p_irc;
            }
            break;
        }
        case OMAP_I: {
            auto p_irc = self.m_repr.pom;
            if (--(std::get<1>(*p_irc).m_fields.ref_count) == 0) {
                for (auto& item : std::get<0>(*p_irc))
                    item.second.clear_parent();
                delete p_irc;
            }
            break;
        }
        case DSRC_I: {
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
    m_fields.repr_ix = EMPTY_I;
}

inline
Object& Object::operator = (const Object& other) {
    dec_ref_count();

    m_fields.repr_ix = other.m_fields.repr_ix;
    switch (m_fields.repr_ix) {
        case EMPTY_I: m_repr.z = nullptr; break;
        case NULL_I:  m_repr.z = nullptr; break;
        case BOOL_I:  m_repr.b = other.m_repr.b; break;
        case INT_I:   m_repr.i = other.m_repr.i; break;
        case UINT_I:  m_repr.u = other.m_repr.u; break;
        case FLOAT_I: m_repr.f = other.m_repr.f; break;
        case STR_I: {
            m_repr.ps = other.m_repr.ps;
            inc_ref_count();
            break;
        }
        case LIST_I: {
            m_repr.pl = other.m_repr.pl;
            inc_ref_count();
            break;
        }
        case MAP_I: {
            m_repr.psm = other.m_repr.psm;
            inc_ref_count();
            break;
        }
        case OMAP_I: {
            m_repr.pom = other.m_repr.pom;
            inc_ref_count();
            break;
        }
        case DSRC_I: {
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
        case EMPTY_I: m_repr.z = nullptr; break;
        case NULL_I:  m_repr.z = nullptr; break;
        case BOOL_I:  m_repr.b = other.m_repr.b; break;
        case INT_I:   m_repr.i = other.m_repr.i; break;
        case UINT_I:  m_repr.u = other.m_repr.u; break;
        case FLOAT_I: m_repr.f = other.m_repr.f; break;
        case STR_I:   m_repr.ps = other.m_repr.ps; break;
        case LIST_I:  m_repr.pl = other.m_repr.pl; break;
        case MAP_I:   m_repr.psm = other.m_repr.psm; break;
        case OMAP_I:  m_repr.pom = other.m_repr.pom; break;
        case DSRC_I:  m_repr.ds = other.m_repr.ds; break;
    }

    other.m_fields.repr_ix = EMPTY_I;
    other.m_repr.z = nullptr; // safe, but may not be necessary

    return *this;
}

template <class DataSourceType>
DataSourceType* Object::data_source() const {
    if (m_fields.repr_ix != DSRC_I) return nullptr;
    return dynamic_cast<DataSourceType*>(m_repr.ds);
}


class LineIterator
{
  public:
    LineIterator(const Object& object) : m_object{object} {}
    LineIterator(Object&& object) : m_object{std::forward<Object>(object)} {}

    LineIterator(const LineIterator&) = default;
    LineIterator(LineIterator&&) = default;

    LineIterator& operator ++ () {
        m_object.refer_to(m_object.parent());
        if (m_object == null) m_object.release();
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


template <class ValueType, typename VisitPred, typename EnterPred>
class TreeIterator
{
  public:
    using TreeRange = Object::TreeRange<ValueType, VisitPred, EnterPred>;

  public:
    TreeIterator(TreeRange& range) : m_range{range} {}

    TreeIterator(TreeRange& range, List& list)
      : m_range{range}
      , m_it{list.begin()}
      , m_end{list.end()}
    {
        m_range.fifo().push_back(Object{null});  // dummy
        enter(list[0]);
        if (!visit(*m_it)) ++(*this);
    }

    TreeIterator& operator ++ () {
        auto& fifo = m_range.fifo();

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
    bool operator == (const TreeIterator& other) const { return m_range.fifo().empty(); }

  private:
    bool visit(const Object& obj) {
        if constexpr (std::is_invocable<VisitPred, const Object&>::value) {
            return m_range.should_visit(obj);
        } else {
            return true;
        }
    }

    void enter(const Object& obj) {
        if constexpr (std::is_invocable<EnterPred, const Object&>::value) {
            if (obj.is_container() && m_range.should_enter(obj))
                m_range.fifo().push_back(obj);
        } else {
            if (obj.is_container())
                m_range.fifo().push_back(obj);
        }
    }

  private:
    TreeRange& m_range;
    ValueIterator m_it;
    ValueIterator m_end;
};

template <class ValueType, typename VisitPred, typename EnterPred>
class Object::TreeRange
{
  public:
    using TreeIterator = TreeIterator<ValueType, VisitPred, EnterPred>;

  public:
    TreeRange(const Object& object, VisitPred&& visit_pred, EnterPred&& enter_pred)
      : m_visit_pred{visit_pred}
      , m_enter_pred{enter_pred}
    {
        m_list.push_back(object);
    }

    TreeIterator begin() { return TreeIterator{*this, m_list}; }
    TreeIterator end()   { return TreeIterator{*this}; }

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
void Object::save(bool quiet) {
    auto tree_range = iter_tree_if(has_data_source, is_fully_cached);
    for (const auto& obj : tree_range)
        obj.m_repr.ds->save(obj, quiet);
}

inline
bool DataSource::is_valid(const Object& target) {
    if (is_sparse()) {
        if (m_cache.is_empty()) read_type(target);
        return !m_failed;
    } else {
        insure_fully_cached(target);
        return !m_failed;
    }
}

inline
void Object::reset() {
    switch (m_fields.repr_ix) {
        case DSRC_I: m_repr.ds->reset(); break;
        default:      break;
    }
}

inline
void Object::reset_key(const Key& key) {
    switch (m_fields.repr_ix) {
        case DSRC_I: m_repr.ds->reset_key(key); break;
        default:      break;
    }
}

inline
void Object::refresh() {
    switch (m_fields.repr_ix) {
        case DSRC_I: m_repr.ds->refresh(); break;
        default:      break;
    }
}

inline
void Object::refresh_key(const Key& key) {
    switch (m_fields.repr_ix) {
        case DSRC_I: m_repr.ds->refresh_key(key); break;
        default:      break;
    }
}

inline
std::ostream& operator<< (std::ostream& ostream, const Object& obj) {
    ostream << obj.to_str();
    return ostream;
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
            case Object::NULL_I: {
                value = read_key(target, key);
                read_set(target, key, value);
                break;
            }
            case Object::DEL_I: {
                value = null;
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
    Mode mode = m_mode;
    if (mode & Mode::INHERIT && m_parent.m_fields.repr_ix == Object::DSRC_I) {
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
    if (!(mode & Mode::WRITE)) throw WriteProtected();
    if (!(mode & Mode::OVERWRITE)) throw OverwriteProtected();

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
void DataSource::del(const Object& target, const Key& key) {
    set_unsaved(true);
    if (is_sparse()) {
        if (m_cache.is_empty()) read_type(target);
        m_cache.get(key).clear_parent();
        m_cache.set(key, Object::DEL_I);
    } else {
        insure_fully_cached(target);
        m_cache.del(key);
    }
}

inline
void DataSource::save(const Object& target, bool quiet) {
    if (!(resolve_mode() & Mode::WRITE)) throw WriteProtected();
    if (m_cache.m_fields.repr_ix == Object::EMPTY_I) return;

    if (m_unsaved) {
        m_unsaved = false;

        if (m_fully_cached) {
            write(target, m_cache, quiet);
        }
        else if (is_sparse()) {
            KeyList deleted_keys;

            for (auto& [key, value] : m_cache.items()) {
                if (value.m_fields.repr_ix == Object::DEL_I) {
                    delete_key(target, key, quiet);
                    deleted_keys.push_back(key);
                } else {
                    write_key(target, key, value, quiet);
                }
            }

            // clear delete records
            for (auto& del_key : deleted_keys)
                m_cache.del(del_key);

            // final save notification to support batching
            commit(target, deleted_keys, quiet);
        }
    }
}

inline
void DataSource::reset() {
    m_fully_cached = false;
    m_failed = false;
    m_cache = Object{m_default_repr_ix};
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
        DEBUG("Loading: {}", target.path().to_str());

        read(target);

        m_fully_cached = true;

        switch (m_cache.m_fields.repr_ix) {
            case Object::LIST_I: {
                for (auto& value : std::get<0>(*m_cache.m_repr.pl))
                    value.set_parent(target);  // TODO: avoid setting parent twice
                break;
            }
            case Object::MAP_I: {
                for (auto& item : std::get<0>(*m_cache.m_repr.psm))
                    item.second.set_parent(target);  // TODO: avoid setting parent twice
                break;
            }
            case Object::OMAP_I: {
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

namespace test {

// Test interface for examining the private DataSource cache.
//
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

//----------------------------------------------------------------------------------
// std::hash
//----------------------------------------------------------------------------------
template<>
struct hash<nodel::OPath>
{
    std::size_t operator () (const nodel::OPath& path) const noexcept {
      return path.hash();
    }
};

} // namespace std
