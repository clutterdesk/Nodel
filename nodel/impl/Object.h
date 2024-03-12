#pragma once

#include <limits>
#include <string>
#include <string_view>
#include <vector>
#include <tsl/ordered_map.h>
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

#include "support.h"
#include "Oid.h"
#include "Key.h"
#include "types.h"

namespace nodel {

struct EmptyReference : std::exception
{
    EmptyReference(const char* func_name) {
        std::stringstream ss;
        ss << "Invalid function call '" << func_name << "' on empty/uninitialized object";
        msg = ss.str();
    }
    const char* what() const noexcept override { return msg.c_str(); }
    std::string msg;
};

struct WriteProtected : std::exception
{
    WriteProtected() = default;
    const char* what() const noexcept override { return "Data-source is write protected"; }
};

struct OverwriteProtected : std::exception
{
    OverwriteProtected() = default;
    const char* what() const noexcept override { return "Data-source is overwrite protected"; }
};

struct InvalidPath : std::exception
{
    InvalidPath() = default;
    const char* what() const noexcept override { return "Invalid object path"; }
};

struct PathSyntax : std::exception
{
    PathSyntax(const char* spec, int64_t offset) {
        std::stringstream ss;
        std::stringstream annot;
        ss << "\n'";
        for (char c = *spec; c != 0 && offset > 0; c = *(++spec), ++offset) {
            ss << c;
            annot << '-';
        }
        ss << '\'';
        annot << '^';
        msg = ss.str() + '\n' + annot.str();
    }
    const char* what() const noexcept override { return msg.c_str(); }
    std::string msg;
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

using String = std::string;
using List = std::vector<Object>;
using Map = tsl::ordered_map<Key, Object, KeyHash>;

using Item = std::pair<Key, Object>;
using ConstItem = std::pair<const Key, const Object>;
using KeyList = std::vector<Key>;
using ItemList = std::vector<Item>;

// inplace reference count types
using IRCString = std::tuple<String, Object>;  // ref-count moved to parent bit-field
using IRCList = std::tuple<List, Object>;      // ref-count moved to parent bit-field
using IRCMap = std::tuple<Map, Object>;        // ref-count moved to parent bit-field

using IRCStringPtr = IRCString*;
using IRCListPtr = IRCList*;
using IRCMapPtr = IRCMap*;
using DataSourcePtr = DataSource*;

constexpr size_t min_key_chunk_size = 128;


// NOTE: use by-value semantics
class Object
{
  public:
    template <class ValueType> class LineageRange;
    template <class ValueType> class ChildrenRange;
    template <class ValueType> class TreeRange;

    // TODO: remove _I suffix
    enum ReprType {
        EMPTY_I,  // uninitialized
        NULL_I,   // json null, also used for non-existent parent
        BOOL_I,
        INT_I,
        UINT_I,
        FLOAT_I,
        STR_I,
        LIST_I,
        SMAP_I,   // TODO small ordered map (also update *Iterator classes)
        OMAP_I,   // ordered map
        RBTREE_I, // TODO (also update *Iterator classes)
        TABLE_I,  // TODO map-like key access, all rows have same keys (ex: CSV) (also update *Iterator classes)
        BIGI_I,   // TODO big integer with
        BIGF_I,   // TODO
        VI4_I,    // TODO
        VI8_I,    // TODO
        VU4_I,    // TODO
        VU8_I,    // TODO
        VF4_I,    // TODO
        VF8_I,    // TODO
        BLOB_I,   // TODO
        DSRC_I,   // DataSource
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
          Repr(IRCMapPtr p)     : pm{p} {}
          Repr(DataSourcePtr p) : ds{p} {}

          void*         z;
          bool          b;
          Int           i;
          UInt          u;
          Float         f;
          IRCStringPtr  ps;
          IRCListPtr    pl;
          IRCMapPtr     pm;
          DataSourcePtr ds;
      };

      struct Fields
      {
          Fields(uint8_t repr_ix)      : repr_ix{repr_ix}, unsaved{0}, ref_count{1} {}
          Fields(const Fields& fields) : repr_ix{fields.repr_ix}, unsaved{fields.unsaved}, ref_count{1} {}
          Fields(Fields&&) = delete;
          auto operator = (const Fields&) = delete;
          auto operator = (Fields&&) = delete;

#if NODEL_ARCH == 32
          uint8_t repr_ix:5;
          uint8_t unsaved: 1;  // data-source update tracking
          uint32_t ref_count:26;
#else
          uint8_t repr_ix:8;
          uint8_t unsaved: 1;  // data-source update tracking
          uint64_t ref_count: 55;
#endif
      };

  private:
    template <typename T> ReprType ix_conv() const;
    template <> ReprType ix_conv<bool>() const   { return BOOL_I; }
    template <> ReprType ix_conv<Int>() const    { return INT_I; }
    template <> ReprType ix_conv<UInt>() const   { return UINT_I; }
    template <> ReprType ix_conv<Float>() const  { return FLOAT_I; }
    template <> ReprType ix_conv<String>() const { return STR_I; }

    template <typename T> T val_conv() const requires is_byvalue<T>;
    template <> bool val_conv<bool>() const     { return m_repr.b; }
    template <> Int val_conv<Int>() const       { return m_repr.i; }
    template <> UInt val_conv<UInt>() const     { return m_repr.u; }
    template <> Float val_conv<Float>() const   { return m_repr.f; }

    template <typename T> T& val_conv() requires is_byvalue<T>;
    template <> bool& val_conv<bool>()     { return m_repr.b; }
    template <> Int& val_conv<Int>()       { return m_repr.i; }
    template <> UInt& val_conv<UInt>()     { return m_repr.u; }
    template <> Float& val_conv<Float>()   { return m_repr.f; }

    template <typename T> const T& val_conv() const requires std::is_same<T, String>::value { return std::get<0>(*m_repr.ps); }
    template <typename T> T& val_conv() requires std::is_same<T, String>::value             { return std::get<0>(*m_repr.ps); }

  private:
    struct NoParent {};
    Object(const NoParent&) : m_repr{}, m_fields{NULL_I} {}  // initialize reference count
    Object(NoParent&&)      : m_repr{}, m_fields{NULL_I} {}  // initialize reference count

  public:
    static std::string_view type_name(uint8_t repr_ix) {
      switch (repr_ix) {
          case EMPTY_I: return "empty";
          case NULL_I:  return "null";
          case BOOL_I:  return "bool";
          case INT_I:   return "int";
          case UINT_I:  return "uint";
          case FLOAT_I: return "float";
          case STR_I:   return "string";
          case LIST_I:  return "list";
          case OMAP_I:  return "map";
          case DSRC_I:  return "data-source";
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
    Object(const char* v)              : m_repr{new IRCString{v, NoParent()}}, m_fields{STR_I} { assert(v != nullptr); }
    Object(bool v)                     : m_repr{v}, m_fields{BOOL_I} {}
    Object(is_like_Float auto v)       : m_repr{(Float)v}, m_fields{FLOAT_I} {}
    Object(is_like_Int auto v)         : m_repr{(Int)v}, m_fields{INT_I} {}
    Object(is_like_UInt auto v)        : m_repr{(UInt)v}, m_fields{UINT_I} {}
    Object(DataSourcePtr ptr)          : m_repr{ptr}, m_fields{DSRC_I} {}  // ownership transferred

    Object(const List&);
    Object(const Map&);
    Object(List&&);
    Object(Map&&);

    Object(const Key&);
    Object(Key&&);

    Object(ReprType type);

    Object(const Object& other);
    Object(Object&& other);

    ~Object() { dec_ref_count(); }

    ReprType type() const;

    Object root() const;
    Object parent() const;

    KeyRange iter_keys() const;
    ItemRange iter_items() const;
    ValueRange iter_values() const;

    LineageRange<Object> iter_lineage() const;
    TreeRange<Object> iter_tree() const;

    template <typename Visitor>
    void iter_visit(Visitor&&) const;

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
    bool is_type() const { return resolve_repr_ix() == ix_conv<T>(); }

    template <typename V>
    void visit(V&& visitor) const;

    template <typename T>
    T as() const requires is_byvalue<T> {
        if (m_fields.repr_ix == ix_conv<T>()) return val_conv<T>();
        else if (m_fields.repr_ix == DSRC_I) return dsrc_read().as<T>();
        throw wrong_type(m_fields.repr_ix);
    }

    template <typename T>
    const T& as() const requires std::is_same<T, String>::value {
        if (m_fields.repr_ix == ix_conv<T>()) return val_conv<T>();
        else if (m_fields.repr_ix == DSRC_I) return dsrc_read().as<T>();
        throw wrong_type(m_fields.repr_ix);
    }

    template <typename T>
    T& as() requires is_byvalue<T> {
        if (m_fields.repr_ix == ix_conv<T>()) return val_conv<T>();
        else if (m_fields.repr_ix == DSRC_I) return dsrc_read().as<T>();
        throw wrong_type(m_fields.repr_ix);
    }

    template <typename T>
    T& as() requires std::is_same<T, String>::value {
        if (m_fields.repr_ix == ix_conv<T>()) return val_conv<T>();
        else if (m_fields.repr_ix == DSRC_I) return dsrc_read().as<T>();
        throw wrong_type(m_fields.repr_ix);
    }

    bool is_empty() const     { return m_fields.repr_ix == EMPTY_I; }
    bool is_null() const      { return resolve_repr_ix() == NULL_I; }
    bool is_bool() const      { return resolve_repr_ix() == BOOL_I; }
    bool is_int() const       { return resolve_repr_ix() == INT_I; }
    bool is_uint() const      { return resolve_repr_ix() == UINT_I; }
    bool is_float() const     { return resolve_repr_ix() == FLOAT_I; }
    bool is_str() const       { return resolve_repr_ix() == STR_I; }
    bool is_num() const       { auto type = resolve_repr_ix(); return type == INT_I || type == UINT_I || type == FLOAT_I; }
    bool is_list() const      { return resolve_repr_ix() == LIST_I; }
    bool is_map() const       { return resolve_repr_ix() == OMAP_I; }
    bool is_container() const { auto type = resolve_repr_ix(); return type == LIST_I || type == OMAP_I; }
    bool is_valid() const;

    bool to_bool() const;
    Int to_int() const;
    UInt to_uint() const;
    Float to_float() const;
    std::string to_str() const;
    Key to_key() const;
    Key to_tmp_key() const;  // returned key is a *view* with lifetime of this
    Key into_key();
    std::string to_json() const;
    void to_json(std::ostream&) const;

//    Object operator [] (is_byvalue auto) const;
//    Object operator [] (const char*) const;
//    Object operator [] (const std::string&) const;

    Object get(is_integral auto v) const;
    Object get(const char* v) const;
    Object get(const std::string& v) const;
    Object get(bool v) const;
    Object get(const Key& key) const;
    Object get(const OPath& path) const;

    void set(is_integral auto, const Object&);
    void set(const char*, const Object&);
    void set(const std::string&, const Object&);
    void set(bool, const Object&);
    void set(const Key&, const Object&);
    void set(Key&&, const Object&);
    void set(std::pair<const Key&, const Object&>);
//    void set(const OPath&, const Object&);

    void del(is_integral auto v);
    void del(const char* v);
    void del(const std::string& v);
    void del(const Key& key);
    void del(const OPath& path);
    void del_from_parent();

    bool operator == (const Object&) const;
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
    // TODO: Need other assignment ops to avoid Object temporaries

    template <class DataSourceType>
    DataSourceType* data_source() const;

    bool has_data_source() const { return m_fields.repr_ix == DSRC_I; }

    void save();
    void reset();
    void reset_key(const Key&);
    void refresh();
    void refresh_key(const Key&);

    static WrongType wrong_type(uint8_t actual)                   { return type_name(actual); };
    static WrongType wrong_type(uint8_t actual, uint8_t expected) { return {type_name(actual), type_name(expected)}; };
    static EmptyReference empty_reference(const char* func_name)  { return func_name; }

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
  friend class KeyRange;
  friend class ValueRange;
  friend class ItemRange;

  friend class WalkDF;
  friend class WalkBF;

  template <class ValueType> friend class LineageIterator;
  template <class ValueType> friend class TreeIterator;
};


class OPath
{
  public:
    using Iterator = KeyList::reverse_iterator;
    using ConstIterator = KeyList::const_reverse_iterator;

    OPath() {}
    OPath(const KeyList& keys)              : m_keys{keys} {}
    OPath(KeyList&& keys)                   : m_keys{std::forward<KeyList>(keys)} {}
    OPath(const Key& key)                   : m_keys{} { append(key); }
    OPath(Key&& key)                        : m_keys{} { append(std::forward<Key>(key)); }

    void prepend(const Key& key) { m_keys.push_back(key); }
    void prepend(Key&& key)      { m_keys.emplace_back(std::forward<Key>(key)); }
    void append(const Key& key)  { m_keys.insert(m_keys.begin(), key); }
    void append(Key&& key)       { m_keys.emplace(m_keys.begin(), std::forward<Key>(key)); }

    OPath parent() const {
        if (m_keys.size() == 0) throw InvalidPath();
        return KeyList{m_keys.begin() + 1, m_keys.end()};
    }

    Object lookup(const Object& origin) const {
        Object obj = origin;
        for (auto& key : *this) {
            auto child = obj.get(key);
            assert(!child.is_empty());
            if (child.is_null()) return {};
            obj.refer_to(child);
        }
        return obj;
    }

    std::string to_str() const {
        std::stringstream ss;
        for (auto& key : *this)
            key.to_step(ss);
        return ss.str();
    }

    Iterator begin() { return m_keys.rbegin(); }
    Iterator end() { return m_keys.rend(); }
    ConstIterator begin() const { return m_keys.crbegin(); }
    ConstIterator end() const { return m_keys.crend(); }

  private:
    KeyList m_keys;

  friend class Object;
};


inline
OPath Object::path() const {
    OPath path;
    Object obj = *this;
    Object par = parent();
    while (!par.is_null()) {
        path.prepend(par.key_of(obj));
        obj.refer_to(par);
        par.refer_to(obj.parent());
    }
    return path;
}

inline
OPath Object::path(const Object& root) const {
    if (root.is_null()) return path();
    OPath path;
    Object obj = *this;
    Object par = parent();
    while (!par.is_null() && !obj.is(root)) {
        path.prepend(par.key_of(obj));
        obj.refer_to(par);
        par.refer_to(obj.parent());
    }
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
        bool done() const { return m_key.is_null(); }

        KeyIterator& begin() { return *this; }
        KeyIterator& end() { return *this; }

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

        ValueIterator& begin() { return *this; }
        ValueIterator& end() { return *this; }

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
        bool done() const { return m_item.first.is_null(); }

        ItemIterator& begin() { return *this; }
        ItemIterator& end() { return *this; }

      protected:
        virtual bool next_impl() = 0;
        Item m_item;
    };

  public:
    using ReprType = Object::ReprType;

    enum class Sparse { SPARSE, COMPLETE };

    constexpr static int READ =      0x1;
    constexpr static int WRITE =     0x2;
    constexpr static int OVERWRITE = 0x4;

    DataSource(Sparse sparse, int mode)                   : m_mode{mode}, m_parent{Object::NULL_I}, m_sparse{sparse} {}
    DataSource(Sparse sparse, int mode, ReprType repr_ix) : m_cache{repr_ix}, m_mode{mode}, m_parent{Object::NULL_I}, m_sparse{sparse} {}
    virtual ~DataSource() {}

    bool is_sparse() const { return m_sparse == Sparse::SPARSE; }

  protected:
    virtual DataSource* new_instance(const Object& target) const = 0;
    virtual void read_meta(const Object& target, Object&)                   {}  // load meta-data including type and id
    virtual void read(const Object& target, Object&) = 0;
    virtual Object read_key(const Object& target, const Key&)               { return {}; }  // sparse
    virtual size_t read_size(const Object& target)                          { return 0; }  // sparse
    virtual void write(const Object& target, const Object&)                 {}  // both
    virtual void write_key(const Object& target, const Key&, const Object&) {}  // sparse
    virtual void write_key(const Object& target, const Key&, Object&&)      {}  // sparse

  public:
    virtual std::unique_ptr<KeyIterator> key_iter()     { return nullptr; }
    virtual std::unique_ptr<ValueIterator> value_iter() { return nullptr; }
    virtual std::unique_ptr<ItemIterator> item_iter()   { return nullptr; }

    virtual std::string to_str(const Object& target) {
        insure_fully_cached(target);
        return m_cache.to_str();
    }

    const Object& get_cached(const Object& target) const { const_cast<DataSource*>(this)->insure_fully_cached(target); return m_cache; }
    Object& get_cached(const Object& target)             { insure_fully_cached(target); return m_cache; }

    size_t size(const Object& target);
    Object get(const Object& target, const Key& key);
    void set(const Object& value);
    void set(const Object& target, const Key& key, const Object& value);
    void set(const Object& target, Key&& key, const Object& value);
    void del(const Object& target, const Key& key);
    void save(const Object& target);

    const Key key_of(const Object& obj) { return m_cache.key_of(obj); }
    ReprType type(const Object& target) { if (m_cache.is_empty()) read_meta(target, m_cache); return m_cache.type(); }
    Oid id(const Object& target)        { if (m_cache.is_empty()) read_meta(target, m_cache); return m_cache.id(); }

    int mode() const        { return m_mode; }
    void set_mode(int mode) { m_mode = mode; }

    void set_failed(bool failed) { m_failed = failed; }
    bool is_valid(const Object& target);

    bool is_fully_cached() const { return m_fully_cached; }

    void reset();
    void reset_key(const Key& key);
    void refresh();
    void refresh_key(const Key& key);

  protected:
    void insure_fully_cached(const Object& target);

  protected:
    Object m_cache;
    int m_mode;

  private:
    refcnt_t ref_count() const { return m_parent.m_fields.ref_count; }
    void inc_ref_count()       { m_parent.m_fields.ref_count++; }
    refcnt_t dec_ref_count()   { return --(m_parent.m_fields.ref_count); }

  private:
    Object m_parent;
    Sparse m_sparse;
    bool m_fully_cached = false;
    bool m_failed = false;

  friend class Object;
};


inline
Object::Object(const Object& other) : m_fields{other.m_fields} {
//        fmt::print("Object(const Object& other)\n");
    switch (m_fields.repr_ix) {
        case EMPTY_I: m_repr.z = nullptr; break;
        case NULL_I:  m_repr.z = nullptr; break;
        case BOOL_I:  m_repr.b = other.m_repr.b; break;
        case INT_I:   m_repr.i = other.m_repr.i; break;
        case UINT_I:  m_repr.u = other.m_repr.u; break;
        case FLOAT_I: m_repr.f = other.m_repr.f; break;
        case STR_I:   other.inc_ref_count(); m_repr.ps = other.m_repr.ps; break;
        case LIST_I:  other.inc_ref_count(); m_repr.pl = other.m_repr.pl; break;
        case OMAP_I:  other.inc_ref_count(); m_repr.pm = other.m_repr.pm; break;
        case DSRC_I:  m_repr.ds = other.m_repr.ds; m_repr.ds->inc_ref_count(); break;
    }
}

inline
Object::Object(Object&& other) : m_fields{other.m_fields} {
//        fmt::print("Object(Object&& other)\n");
    switch (m_fields.repr_ix) {
        case EMPTY_I: m_repr.z = nullptr; break;
        case NULL_I:  m_repr.z = nullptr; break;
        case BOOL_I:  m_repr.b = other.m_repr.b; break;
        case INT_I:   m_repr.i = other.m_repr.i; break;
        case UINT_I:  m_repr.u = other.m_repr.u; break;
        case FLOAT_I: m_repr.f = other.m_repr.f; break;
        case STR_I:   m_repr.ps = other.m_repr.ps; break;
        case LIST_I:  m_repr.pl = other.m_repr.pl; break;
        case OMAP_I:  m_repr.pm = other.m_repr.pm; break;
        case DSRC_I:  m_repr.ds = other.m_repr.ds; break;
    }

    other.m_fields.repr_ix = EMPTY_I;
    other.m_repr.z = nullptr;
}

inline
Object::Object(const List& list) : m_repr{new IRCList({}, NoParent{})}, m_fields{LIST_I} {
    List& my_list = std::get<0>(*m_repr.pl);
    for (auto& value : list) {
        Object copy = value.copy();
        my_list.push_back(copy);
        copy.set_parent(*this);
    }
}

inline
Object::Object(const Map& map) : m_repr{new IRCMap({}, NoParent{})}, m_fields{OMAP_I} {
    Map& my_map = std::get<0>(*m_repr.pm);
    for (auto& [key, value]: map) {
        Object copy = value.copy();
        my_map.insert({key, copy});
        copy.set_parent(*this);
    }
}

inline
Object::Object(List&& list) : m_repr{new IRCList(std::forward<List>(list), NoParent{})}, m_fields{LIST_I} {
    List& my_list = std::get<0>(*m_repr.pl);
    for (auto& obj : my_list)
        obj.set_parent(*this);
}

inline
Object::Object(Map&& map) : m_repr{new IRCMap(std::forward<Map>(map), NoParent{})}, m_fields{OMAP_I} {
    Map& my_map = std::get<0>(*m_repr.pm);
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
            m_repr.ps = new IRCString{key.m_repr.s, {}};
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
            m_repr.ps = new IRCString{std::move(key.m_repr.s), {}};
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
        case STR_I:   m_repr.ps = new IRCString{"", {}}; break;
        case LIST_I:  m_repr.pl = new IRCList{{}, {}}; break;
        case OMAP_I:  m_repr.pm = new IRCMap{{}, {}}; break;
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
    while (!par.is_null()) {
        obj.refer_to(par);
        par.refer_to(par.parent());
    }
    return obj;
}

inline
Object Object::parent() const {
    switch (m_fields.repr_ix) {
        case EMPTY_I: throw empty_reference(__FUNCTION__);
        case STR_I:   return std::get<1>(*m_repr.ps);
        case LIST_I:  return std::get<1>(*m_repr.pl);
        case OMAP_I:  return std::get<1>(*m_repr.pm);
        case DSRC_I:  return m_repr.ds->m_parent;
        default:      return null;
    }
}

template <typename V>
void Object::visit(V&& visitor) const {
    switch (m_fields.repr_ix) {
        case EMPTY_I: throw empty_reference(__FUNCTION__);
        case NULL_I:  visitor(nullptr); break;
        case BOOL_I:  visitor(m_repr.b); break;
        case INT_I:   visitor(m_repr.i); break;
        case UINT_I:  visitor(m_repr.u); break;
        case FLOAT_I: visitor(m_repr.f); break;
        case STR_I:   visitor(std::get<0>(*m_repr.ps)); break;
        case DSRC_I:  visitor(*this); break;
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
    dec_ref_count();

    m_fields.repr_ix = other.m_fields.repr_ix;
    switch (m_fields.repr_ix) {
        case EMPTY_I: throw empty_reference(__FUNCTION__);
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
        case OMAP_I: {
            m_repr.pm = other.m_repr.pm;
            inc_ref_count();
            break;
        }
        case DSRC_I: {
            m_repr.ds = other.m_repr.ds;
            inc_ref_count();
            break;
        }
    }
}

inline
void Object::weak_refer_to(const Object& other) {
    // NOTE: This method is private, and only intended for use by the set_parent method.
    //       It's part of a scheme for breaking the parent/child reference cycle.
    m_fields.repr_ix = other.m_fields.repr_ix;
    switch (m_fields.repr_ix) {
        case EMPTY_I: throw empty_reference(__FUNCTION__);
        case NULL_I:  m_repr.z = nullptr; break;
        case BOOL_I:  m_repr.b = other.m_repr.b; break;
        case INT_I:   m_repr.i = other.m_repr.i; break;
        case UINT_I:  m_repr.u = other.m_repr.u; break;
        case FLOAT_I: m_repr.f = other.m_repr.f; break;
        case STR_I:   m_repr.ps = other.m_repr.ps; break;
        case LIST_I:  m_repr.pl = other.m_repr.pl; break;
        case OMAP_I:  m_repr.pm = other.m_repr.pm; break;
        case DSRC_I:  m_repr.ds = other.m_repr.ds; break;
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
        case OMAP_I: {
            auto& parent = std::get<1>(*m_repr.pm);
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
        case OMAP_I: {
            auto& parent = std::get<1>(*m_repr.pm);
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
                    return Key{index};
                index++;
            }
            break;
        }
        case OMAP_I: {
            const auto& map = std::get<0>(*m_repr.pm);
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
    return Key{};
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
        case EMPTY_I: throw empty_reference(__FUNCTION__);
        case NULL_I:  return "null";
        case BOOL_I:  return m_repr.b? "true": "false";
        case INT_I:   return int_to_str(m_repr.i);
        case UINT_I:  return int_to_str(m_repr.u);
        case FLOAT_I: return nodel::float_to_str(m_repr.f);
        case STR_I:   return std::get<0>(*m_repr.ps);
        case LIST_I:
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
        case EMPTY_I: throw empty_reference(__FUNCTION__);
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
        case EMPTY_I: throw empty_reference(__FUNCTION__);
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
        case EMPTY_I: throw empty_reference(__FUNCTION__);
        case NULL_I:  throw wrong_type(m_fields.repr_ix, UINT_I);
        case BOOL_I:  return m_repr.b;
        case INT_I:   return m_repr.i;
        case UINT_I:  return m_repr.u;
        case FLOAT_I: return m_repr.f;
        case STR_I:   return str_to_int(std::get<0>(*m_repr.ps));
        case DSRC_I:  return const_cast<DataSourcePtr>(m_repr.ds)->get_cached(*this).to_uint();
        default:      throw wrong_type(m_fields.repr_ix, UINT_I);
    }
}

inline
Float Object::to_float() const {
    switch (m_fields.repr_ix) {
        case EMPTY_I: throw empty_reference(__FUNCTION__);
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
        case EMPTY_I: throw empty_reference(__FUNCTION__);
        case NULL_I:  return nullptr;
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
Key Object::to_tmp_key() const {
    switch (m_fields.repr_ix) {
        case EMPTY_I: throw empty_reference(__FUNCTION__);
        case NULL_I:  return nullptr;
        case BOOL_I:  return m_repr.b;
        case INT_I:   return m_repr.i;
        case UINT_I:  return m_repr.u;
        case FLOAT_I: return m_repr.f;
        case STR_I:   return std::get<0>(*m_repr.ps);
        case DSRC_I:  return const_cast<DataSourcePtr>(m_repr.ds)->get_cached(*this).to_tmp_key();
        default:      throw wrong_type(m_fields.repr_ix);
    }
}

inline
Key Object::into_key() {
    switch (m_fields.repr_ix) {
        case EMPTY_I: throw empty_reference(__FUNCTION__);
        case NULL_I:  { return nullptr; }
        case BOOL_I:  { return {m_repr.b}; }
        case INT_I:   { return {m_repr.i}; }
        case UINT_I:  { return {m_repr.u}; }
        case FLOAT_I: { return {m_repr.f}; }
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

//inline Object Object::operator [] (is_byvalue auto v) const {
//    return get(v);
//}
//
//inline Object Object::operator [] (const char* k) const {
//    return get(k);
//}
//
//inline Object Object::operator [] (const std::string& k) const {
//    return get(k);
//}

inline
Object Object::get(const char* v) const {
    switch (m_fields.repr_ix) {
        case EMPTY_I: throw empty_reference(__FUNCTION__);
        case OMAP_I: {
            auto& map = std::get<0>(*m_repr.pm);
            auto it = map.find(v);
            if (it == map.end()) return null;
            return it->second;
        }
        case DSRC_I:  return m_repr.ds->get(*this, v);
        default:      throw wrong_type(m_fields.repr_ix);
    }
}

inline
Object Object::get(const std::string& v) const {
    switch (m_fields.repr_ix) {
        case EMPTY_I: throw empty_reference(__FUNCTION__);
        case OMAP_I: {
            auto& map = std::get<0>(*m_repr.pm);
            auto it = map.find(v);
            if (it == map.end()) return null;
            return it->second;
        }
        case DSRC_I:  return m_repr.ds->get(*this, v);
        default:      throw wrong_type(m_fields.repr_ix);
    }
}

inline
Object Object::get(is_integral auto index) const {
    switch (m_fields.repr_ix) {
        case EMPTY_I: throw empty_reference(__FUNCTION__);
        case LIST_I: {
            auto& list = std::get<0>(*m_repr.pl);
            if (index < 0) index += list.size();
            if (index < 0 || index >= list.size()) return null;
            return list[index];
        }
        case OMAP_I: {
            auto& map = std::get<0>(*m_repr.pm);
            auto it = map.find(index);
            if (it == map.end()) return null;
            return it->second;
        }
        case DSRC_I:  return m_repr.ds->get(*this, index);
        default:      throw wrong_type(m_fields.repr_ix);
    }
}

inline
Object Object::get(bool v) const {
    return get(Key{v});
}

inline
Object Object::get(const Key& key) const {
    switch (m_fields.repr_ix) {
        case EMPTY_I: throw empty_reference(__FUNCTION__);
        case LIST_I: {
            auto& list = std::get<0>(*m_repr.pl);
            Int index = key.to_int();
            if (index < 0) index += list.size();
            if (index < 0 || index >= list.size()) return null;
            return list[index];
        }
        case OMAP_I: {
            auto& map = std::get<0>(*m_repr.pm);
            auto it = map.find(key);
            if (it == map.end()) return null;
            return it->second;
        }
        case DSRC_I:  return m_repr.ds->get(*this, key);
        default:
            throw wrong_type(m_fields.repr_ix);
    }
}

inline
Object Object::get(const OPath& path) const {
    return path.lookup(*this);
}

inline
void Object::set(is_integral auto key, const Object& value) {
    set(Key{key}, value);
}

inline
void Object::set(const char* key, const Object& value) {
    set(Key{key}, value);
}

inline
void Object::set(const std::string& key, const Object& value) {
    set(Key{key}, value);
}

inline
void Object::set(bool key, const Object& value) {
    set(Key{key}, value);
}

inline
void Object::set(const Key& key, const Object& c_value) {
    switch (m_fields.repr_ix) {
        case EMPTY_I: throw empty_reference(__FUNCTION__);
        case LIST_I: {
            Object value = c_value;
            if (!value.parent().is_null()) value.refer_to(value.copy());
            auto& list = std::get<0>(*m_repr.pl);
            Int index = key.to_int();
            auto size = list.size();
            if (index < 0) index += size;
            if (index >= size) {
                list.push_back(value);
            } else {
                auto it = list.begin() + index;
                it->set_parent(null);
                *it = value;
            }
            value.set_parent(*this);
            break;
        }
        case OMAP_I: {
            Object value = c_value;
            if (!value.parent().is_null()) value.refer_to(value.copy());
            auto& map = std::get<0>(*m_repr.pm);
            auto it = map.find(key);
            if (it != map.end()) {
                it->second.set_parent(null);
                map.erase(it);
            }
            map.insert({key, value});
            value.set_parent(*this);
            break;
        }
        case DSRC_I: return m_repr.ds->set(*this, key, c_value);
        default:
            throw wrong_type(m_fields.repr_ix);
    }
}

inline
void Object::set(Key&& key, const Object& c_value) {
    switch (m_fields.repr_ix) {
        case EMPTY_I: throw empty_reference(__FUNCTION__);
        case LIST_I: {
            Object value = c_value;
            if (!value.parent().is_null()) value.refer_to(value.copy());
            auto& list = std::get<0>(*m_repr.pl);
            Int index = key.to_int();
            auto size = list.size();
            if (index < 0) index += size;
            if (index >= size) {
                list.push_back(value);
            } else {
                auto it = list.begin() + index;
                it->set_parent(null);
                *it = value;
            }
            value.set_parent(*this);
            break;
        }
        case OMAP_I: {
            Object value = c_value;
            if (!value.parent().is_null()) value.refer_to(value.copy());
            auto& map = std::get<0>(*m_repr.pm);
            auto it = map.find(key);
            if (it != map.end()) {
                it->second.set_parent(null);
                map.erase(it);
            }
            map.insert({std::forward<Key>(key), value});
            value.set_parent(*this);
            break;
        }
        case DSRC_I: return m_repr.ds->set(*this, std::forward<Key>(key), c_value);
        default:
            throw wrong_type(m_fields.repr_ix);
    }
}

inline
void Object::set(std::pair<const Key&, const Object&> item) {
    set(item.first, item.second);
}

//inline
//void Object::set(const OPath& path, const Object& value) {
//     create intermediates
//}

inline
void Object::del(is_integral auto v) {
    del(Key{v});
}

inline
void Object::del(const char* v) {
    del(Key{v});
}

inline
void Object::del(const std::string& v) {
    del(Key{v});
}

inline
void Object::del(const Key& key) {
    switch (m_fields.repr_ix) {
        case EMPTY_I: throw empty_reference(__FUNCTION__);
        case LIST_I: {
            auto& list = std::get<0>(*m_repr.pl);
            Int index = key.to_int();
            auto size = list.size();
            if (index < 0) index += size;
            if (index >= 0) {
                if (index > size) index = size;
                list.erase(list.begin() + index);
            }
            break;
        }
        case OMAP_I: {
            auto& map = std::get<0>(*m_repr.pm);
            auto it = map.find(key);
            if (it != map.end()) {
                auto value = it->second;
                value.set_parent(null);
                map.erase(it);
            }
            break;
        }
        case DSRC_I: m_repr.ds->del(*this, key);
        default:     throw wrong_type(m_fields.repr_ix);
    }
}

inline
void Object::del(const OPath& path) {
    auto obj = path.lookup(*this);
    if (!obj.is_null()) {
        auto par = obj.parent();
        par.del(par.key_of(obj));
    }
}

inline
void Object::del_from_parent() {
    Object par = parent();
    if (!par.is_null())
        par.del(par.key_of(*this));
}

inline
size_t Object::size() const {
    switch (m_fields.repr_ix) {
        case EMPTY_I: throw empty_reference(__FUNCTION__);
        case STR_I:   return std::get<0>(*m_repr.ps).size();
        case LIST_I:  return std::get<0>(*m_repr.pl).size();
        case OMAP_I:  return std::get<0>(*m_repr.pm).size();
        case DSRC_I:  return m_repr.ds->size(*this);
        default:
            return 0;
    }
}

#include "KeyRange.h"
#include "ValueRange.h"
#include "ItemRange.h"

// TODO: visit function constraints
template <typename VisitFunc>
void Object::iter_visit(VisitFunc&& visit) const {
    switch (m_fields.repr_ix) {
        case NULL_I:  visit(Object{null}); break;
        case BOOL_I:  visit(m_repr.b); break;
        case INT_I:   visit(m_repr.i); break;
        case UINT_I:  visit(m_repr.u); break;
        case FLOAT_I: visit(m_repr.f); break;
        case STR_I: {
            for (auto c : std::get<0>(*m_repr.ps)) {
                if (!visit(c))
                    break;
            }
            return;
        }
        case LIST_I: {
            for (const auto& obj : std::get<0>(*m_repr.pl)) {
                if (!visit(obj))
                    break;
            }
            return;
        }
        case OMAP_I: {
            auto& map = std::get<0>(*m_repr.pm);
            for (const auto& item : map) {
                if (!visit(std::get<0>(item)))
                    break;
            }
            return;
        }
        case DSRC_I: {
            auto& dsrc = *m_repr.ds;
            if (dsrc.is_sparse()) {
                auto type = dsrc.type(*this);
                assert (type == OMAP_I);
                KeyRange range{*this};
                for (auto val : range)
                    if (!visit(val))
                        break;
            } else {
                dsrc.get_cached(*this).iter_visit(visit);
            }
            return;
        }
        default:
            throw wrong_type(m_fields.repr_ix);
    }
}

inline KeyRange Object::iter_keys() const {
    return *this;
}

inline ItemRange Object::iter_items() const {
    return *this;
}

inline ValueRange Object::iter_values() const {
    return *this;
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
        case OMAP_I:  {
            for (auto& item : std::get<0>(*m_repr.pm))
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
        case OMAP_I:  {
            for (auto& item : std::get<0>(*m_repr.pm))
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
    if (is_empty() || obj.is_empty()) throw empty_reference(__FUNCTION__);
    if (is(obj)) return true;

    switch (m_fields.repr_ix) {
        case NULL_I: {
            if (obj.m_fields.repr_ix == NULL_I) return true;
            throw wrong_type(m_fields.repr_ix);
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
std::partial_ordering Object::operator <=> (const Object& obj) const {
    using ordering = std::partial_ordering;

    if (is_empty() || obj.is_empty()) throw empty_reference(__FUNCTION__);
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
        case DSRC_I: return m_repr.ds->get_cached(*this) <=> obj;
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
    using Visitor = std::function<void(const Object&, const Key&, const Object&, uint8_t)>;

public:
    WalkDF(Object root, Visitor visitor)
    : m_visitor{visitor}
    {
        if (root.is_empty()) throw root.empty_reference(__FUNCTION__);
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
                    case Object::OMAP_I: {
                        m_visitor(parent, key, object, event | BEGIN_PARENT);
                        m_stack.emplace(parent, key, object, event | END_PARENT);
                        auto& map = std::get<0>(*object.m_repr.pm);
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
    Visitor m_visitor;
    Stack m_stack;
};


class WalkBF
{
public:
    using Item = std::tuple<Object, Key, Object>;
    using Deque = std::deque<Item>;
    using Visitor = std::function<void(const Object&, const Key&, const Object&)>;

public:
    WalkBF(Object root, Visitor visitor)
    : m_visitor{visitor}
    {
        if (root.is_empty()) throw root.empty_reference(__FUNCTION__);
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
                case Object::OMAP_I: {
                    auto& map = std::get<0>(*object.m_repr.pm);
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
    Visitor m_visitor;
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
            case EMPTY_I: throw empty_reference(__FUNCTION__);
            case NULL_I:  os << "null"; break;
            case BOOL_I:  os << (object.m_repr.b? "true": "false"); break;
            case INT_I:   os << int_to_str(object.m_repr.i); break;
            case UINT_I:  os << int_to_str(object.m_repr.u); break;
            case FLOAT_I: os << float_to_str(object.m_repr.f); break;
            case STR_I:   os << std::quoted(std::get<0>(*object.m_repr.ps)); break;
            case LIST_I:  os << ((event & WalkDF::BEGIN_PARENT)? '[': ']'); break;
            case OMAP_I:  os << ((event & WalkDF::BEGIN_PARENT)? '{': '}'); break;
            default:      throw wrong_type(object.m_fields.repr_ix);
        }
    };

    WalkDF walk{*this, visitor};
    while (walk.next());
}

inline
Oid Object::id() const {
    switch (m_fields.repr_ix) {
        case EMPTY_I: throw empty_reference(__FUNCTION__);
        case NULL_I:  return Oid::null();
        case BOOL_I:  return Oid{1, m_repr.b};
        case INT_I:   return Oid{2, *((uint64_t*)(&m_repr.i))};
        case UINT_I:  return Oid{3, m_repr.u};;
        case FLOAT_I: return Oid{4, *((uint64_t*)(&m_repr.f))};;
        case STR_I:   return Oid{5, (uint64_t)(&(std::get<0>(*m_repr.ps)))};
        case LIST_I:  return Oid{6, (uint64_t)(&(std::get<0>(*m_repr.pl)))};
        case OMAP_I:  return Oid{7, (uint64_t)(&(std::get<0>(*m_repr.pm)))};
        case DSRC_I:  return Oid{8, (uint64_t)(m_repr.ds)};
        default:      throw wrong_type(m_fields.repr_ix);
    }
}

inline
bool Object::is(const Object& other) const {
    auto repr_ix = m_fields.repr_ix;
    if (other.m_fields.repr_ix != repr_ix) return false;
    switch (repr_ix) {
        case EMPTY_I: throw empty_reference(__FUNCTION__);
        case NULL_I:  return true;
        case BOOL_I:  return m_repr.b == other.m_repr.b;
        case INT_I:   return m_repr.i == other.m_repr.i;
        case UINT_I:  return m_repr.u == other.m_repr.u;
        case FLOAT_I: return m_repr.f == other.m_repr.f;
        case STR_I:   return m_repr.ps == other.m_repr.ps;
        case LIST_I:  return m_repr.pl == other.m_repr.pl;
        case OMAP_I:  return m_repr.pm == other.m_repr.pm;
        case DSRC_I:  return m_repr.ds == other.m_repr.ds;
        default:      throw wrong_type(m_fields.repr_ix);
    }
}

inline
Object Object::copy() const {
    switch (m_fields.repr_ix) {
        case EMPTY_I: throw empty_reference(__FUNCTION__);
        case NULL_I:  return NULL_I;
        case BOOL_I:  return m_repr.b;
        case INT_I:   return m_repr.i;
        case UINT_I:  return m_repr.u;
        case FLOAT_I: return m_repr.f;
        case STR_I:   return std::get<0>(*m_repr.ps);
        case LIST_I:  return std::get<0>(*m_repr.pl);  // TODO: long-hand deep-copy, instead of using stack
        case OMAP_I:  return std::get<0>(*m_repr.pm);  // TODO: long-hand deep-copy, instead of using stack
        case DSRC_I:  return m_repr.ds->new_instance(*this);
        default:      throw wrong_type(m_fields.repr_ix);
    }
}

inline
refcnt_t Object::ref_count() const {
    switch (m_fields.repr_ix) {
        case STR_I:   return std::get<1>(*m_repr.ps).m_fields.ref_count;
        case LIST_I:  return std::get<1>(*m_repr.pl).m_fields.ref_count;
        case OMAP_I:  return std::get<1>(*m_repr.pm).m_fields.ref_count;
        case DSRC_I:  return m_repr.ds->ref_count();
        default:      return no_ref_count;
    }
}


inline
void Object::inc_ref_count() const {
    Object& self = *const_cast<Object*>(this);
    switch (m_fields.repr_ix) {
        case EMPTY_I: throw empty_reference(__FUNCTION__);
        case STR_I:   ++(std::get<1>(*self.m_repr.ps).m_fields.ref_count); break;
        case LIST_I:  ++(std::get<1>(*self.m_repr.pl).m_fields.ref_count); break;
        case OMAP_I:  ++(std::get<1>(*self.m_repr.pm).m_fields.ref_count); break;
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
        case OMAP_I: {
            auto p_irc = self.m_repr.pm;
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
//    fmt::print("Object::operator = (const Object&)\n");
    if (m_fields.repr_ix == DSRC_I) {
        m_repr.ds->set(other);
    } else {
        dec_ref_count();

        m_fields.unsaved = other.m_fields.unsaved;
        m_fields.repr_ix = other.m_fields.repr_ix;
        switch (m_fields.repr_ix) {
            case EMPTY_I: throw empty_reference(__FUNCTION__);
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
            case OMAP_I: {
                m_repr.pm = other.m_repr.pm;
                inc_ref_count();
                break;
            }
            case DSRC_I: {
                m_repr.ds = other.m_repr.ds;
                inc_ref_count();
                break;
            }
        }
    }
    return *this;
}

inline
Object& Object::operator = (Object&& other) {
//    fmt::print("Object::operator = (Object&&)\n");
    if (m_fields.repr_ix == DSRC_I) {
        m_repr.ds->set(other);
    } else {
        dec_ref_count();

        m_fields.unsaved = other.m_fields.unsaved;
        m_fields.repr_ix = other.m_fields.repr_ix;
        switch (m_fields.repr_ix) {
            case EMPTY_I: throw empty_reference(__FUNCTION__);
            case NULL_I:  m_repr.z = nullptr; break;
            case BOOL_I:  m_repr.b = other.m_repr.b; break;
            case INT_I:   m_repr.i = other.m_repr.i; break;
            case UINT_I:  m_repr.u = other.m_repr.u; break;
            case FLOAT_I: m_repr.f = other.m_repr.f; break;
            case STR_I:   m_repr.ps = other.m_repr.ps; break;
            case LIST_I:  m_repr.pl = other.m_repr.pl; break;
            case OMAP_I:  m_repr.pm = other.m_repr.pm; break;
            case DSRC_I:  m_repr.ds = other.m_repr.ds; break;
        }
    }

    other.m_fields.repr_ix = EMPTY_I;
    other.m_repr.z = nullptr; // safe, but may not be necessary

    return *this;
}

template <class DataSourceType>
DataSourceType* Object::data_source() const {
    if constexpr (std::is_same<DataSourceType, DataSource>::value) {
        return (m_fields.repr_ix == DSRC_I)?
               dynamic_cast<DataSource*>(m_repr.ds):
               nullptr;
    } else {
        return (m_fields.repr_ix == DSRC_I && typeid(*m_repr.ds) == typeid(DataSourceType))?
                dynamic_cast<DataSourceType*>(m_repr.ds):
                nullptr;
    }
}


template <typename ValueType>
class LineageIterator
{
  public:
    LineageIterator(const Object& object) : m_object{object} {}
    LineageIterator(Object&& object) : m_object{std::forward<Object>(object)} {}

    LineageIterator(const LineageIterator&) = default;
    LineageIterator(LineageIterator&&) = default;

    LineageIterator<ValueType>& operator ++ () {
        m_object.refer_to(m_object.parent());
        if (m_object.is_null()) m_object.release();
        return *this;
    }
    Object operator * () const { return m_object; }
    bool operator == (const LineageIterator& other) const {
        // m_object may be empty - see LineageRange::end
        if (m_object.is_empty()) return other.m_object.is_empty();
        return !other.m_object.is_empty() && m_object.is(other.m_object);
    }
  private:
    Object m_object;
};

template <typename ValueType>
class Object::LineageRange
{
  public:
    using LineageIterator = LineageIterator<ValueType>;

    LineageRange(const Object& object) : m_object{object} {}
    LineageIterator begin() const { return m_object; }
    LineageIterator end() const   { return Object{}; }
  private:
    Object m_object;
};

inline Object::LineageRange<Object> Object::iter_lineage() const {
    return *this;
}


template <class ValueType>
class TreeIterator
{
  public:
    TreeIterator() = default;

    TreeIterator(const Object& object) {
        m_fifo.push_back(object);
        m_it = m_fifo.front().begin();
        m_end = m_fifo.front().end();
    }

    TreeIterator(List& list) : m_it{list.begin()}, m_end{list.end()} {
        m_fifo.push_back(Object{null});  // dummy
        m_fifo.push_back(list[0]);
    }

    TreeIterator& operator ++ () {
        ++m_it;
        while (m_it == m_end) {
            m_fifo.pop_front();
            if (m_fifo.empty()) break;
            m_it = m_fifo.front().begin();
            m_end = m_fifo.front().end();
        }
        if (m_it != m_end) {
            const Object& obj = *m_it;
            if (obj.is_container() || obj.has_data_source())
                m_fifo.push_back(*m_it);
        }
        return *this;
    }

    Object operator * () const { return *m_it; }
    bool operator == (const TreeIterator& other) const { return m_fifo.empty(); }

  private:
    std::deque<ValueRange> m_fifo;
    ValueIterator m_it;
    ValueIterator m_end;
};

template <class ValueType>
class Object::TreeRange
{
  public:
    TreeRange(const Object& object) { m_list.push_back(object); }
    TreeIterator<ValueType> begin() const { return const_cast<List&>(m_list); }
    TreeIterator<ValueType> end() const   { return {}; }

  private:
    List m_list;
};

inline Object::TreeRange<Object> Object::iter_tree() const { return *this; }


inline
void Object::save() {
    auto tree_range = iter_tree();
    for (auto obj : tree_range) {
        if (obj.m_fields.repr_ix == DSRC_I) {
            obj.m_repr.ds->save(*this);
        }
    }
}

inline
bool DataSource::is_valid(const Object& target) {
    if (is_sparse()) {
        if (m_cache.is_empty()) read_meta(target, m_cache);
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

//
//inline
//OPath::OPath(const char* spec, char delimiter) {
//    auto start = spec;
//    auto it = start;
//    auto c = *it;
//    for (; c != 0; c = *(++it)) {
//        if (c == '[') {
//            if (it != start) key_list.push_back(std::string{start, it});
//            ++it;
//            char *end;
//            key_list.push_back(strtoll(it, &end, 10));
//            fmt::print("c={} dist={}\n", *end, (end - spec));
//            if (end == it || *end == 0 || errno == ERANGE) throw PathSyntax{spec, it - spec};
//            if (*end != ']') throw PathSyntax(spec, it - spec);
//            it = ++end;
//            start = it;
//        } else if (c == delimiter) {
//            key_list.push_back(std::string{start, it});
//            start = ++it;
//        }
//    }
//}
//
//inline
//void OPath::parse(const char* spec) {
//}
//
//inline
//void OPath::parse_step(const char* spec, const char*& it, char delimiter) {
//    while (auto c == *it, std::isspace(c) && c != 0) ++it;  // consume whitespace
//
//    auto c = *it;
//    if (c != delimiter && c != '[')
//        throw PathSyntax(spec, it - spec);
//
//    parse_key(spec, ++it, delimiter);
//}
//
//inline
//void OPath::parse_key(const char* spec, const char*& it, char delimiter) {
//    while (auto c == *it, std::isspace(c) && c != 0) ++it;  // consume whitespace
//
//    if (c == '"') {
//        parse_squote(spec, it, delimiter);
//
//    std::string key;
//    for (auto c = *it; c != 0 && c != delimiter && c != ']'; c = *(++it)) {
//        key.push_back(c);
//    }
//}

inline
size_t DataSource::size(const Object& target) {
    if (is_sparse()) {
        return read_size(target );
    } else {
        insure_fully_cached(target);
        return m_cache.size();
    }
}

inline
Object DataSource::get(const Object& target, const Key& key) {
    if (is_sparse()) {
        if (m_cache.is_empty()) { read_meta(target, m_cache); }

        // empty value means "not cached" and null value means "cached, but no value"
        Object value = m_cache.get(key);
        if (value.is_null()) {
            value = read_key(target, key);
            m_cache.set(key, value);
            value.set_parent(target);  // TODO: avoid setting parent twice
        }

        return value;
    } else {
        insure_fully_cached(target);
        return m_cache.get(key);
    }
}

inline
void DataSource::set(const Object& value) {
    if (!(m_mode & WRITE)) throw WriteProtected();
    if (!(m_mode & OVERWRITE)) throw OverwriteProtected();
    const_cast<Object&>(value).m_fields.unsaved = 1;
    m_cache = value;
    m_fully_cached = true;
}

inline
void DataSource::set(const Object& target, const Key& key, const Object& value) {
    if (!(m_mode & WRITE)) throw WriteProtected();
    const_cast<Object&>(value).m_fields.unsaved = 1;
    if (is_sparse()) {
        if (m_cache.is_empty()) read_meta(target, m_cache);
        m_cache.set(key, value);
    } else {
        insure_fully_cached(target);
        m_cache.set(key, value);
    }
    value.set_parent(target);  // TODO: avoid setting parent twice
}

inline
void DataSource::set(const Object& target, Key&& key, const Object& value) {
    if (!(m_mode & WRITE)) throw WriteProtected();
    const_cast<Object&>(value).m_fields.unsaved = 1;
    if (is_sparse()) {
        if (m_cache.is_empty()) read_meta(target, m_cache);
        m_cache.set(std::forward<Key>(key), value);
    } else {
        insure_fully_cached(target);
        m_cache.set(std::forward<Key>(key), value);
    }
    value.set_parent(target);  // TODO: avoid setting parent twice
}

inline
void DataSource::del(const Object& target, const Key& key) {
    if (!(m_mode & WRITE)) throw WriteProtected();
    if (is_sparse()) {
        if (m_cache.is_empty()) read_meta(target, m_cache);
        m_cache.set(key, null);
    } else {
        insure_fully_cached(target);
        m_cache.del(key);
    }
}

inline
void DataSource::save(const Object& target) {
    if (m_cache.is_empty()) return;

    if (m_fully_cached && m_cache.m_fields.unsaved) {
        m_cache.m_fields.unsaved = 0;
        write(target, m_cache);
    }
    else if (is_sparse()) {
        KeyList del_keys;

        for (auto& [key, value] : m_cache.items()) {
            if (value.m_fields.unsaved) {
                const_cast<Object&>(value).m_fields.unsaved = 0;
                write_key(target, key, value);
            } else if (value.is_null()) {
                write_key(target, key, value);
                del_keys.push_back(key);
            }
        }

        // clear delete records
        for (auto& del_key : del_keys)
            m_cache.del(del_key);
    }
}

inline
void DataSource::reset() {
    m_fully_cached = false;
    m_failed = false;
    m_cache.release();
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
        read(target, m_cache);
        m_fully_cached = true;
        switch (m_cache.m_fields.repr_ix) {
            case Object::LIST_I: {
                for (auto& value : std::get<0>(*m_cache.m_repr.pl))
                    value.set_parent(target);  // TODO: avoid setting parent twice
                break;
            }
            case Object::OMAP_I: {
                for (auto& item : std::get<0>(*m_cache.m_repr.pm))
                    item.second.set_parent(target);  // TODO: avoid setting parent twice
                break;
            }
            default:
                break;
        }
    }
}

} // nodel namespace

#ifdef FMT_FORMAT_H_

namespace fmt {

template <>
struct formatter<nodel::Object> : formatter<const char*> {
    auto format(const nodel::Object& obj, format_context& ctx) {
        std::string str = obj.to_str();
        return fmt::formatter<const char*>::format((const char*)str.c_str(), ctx);
    }
};

template <>
struct formatter<nodel::OPath> : formatter<const char*> {
    auto format(const nodel::OPath& path, format_context& ctx) {
        std::string str = path.to_str();
        return fmt::formatter<const char*>::format((const char*)str.c_str(), ctx);
    }
};

} // namespace fmt

#endif
