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

#include "support.h"
#include "Oid.h"
#include "Key.h"


#ifndef NODEL_ARCH
#if _WIN32 || _WIN64
#if _WIN64
#define NODEL_ARCH 64
#else
#define NODEL_ARCH 32
#endif
#endif

#if __GNUC__
#if __x86_64__ || __ppc64__
#define NODEL_ARCH 64
#else
#define NODEL_ARCH 32
#endif
#endif
#endif // NODEL_ARCH


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

struct ReadOnly : std::exception
{
    ReadOnly() = default;
    const char* what() const noexcept override { return "Data-source is read-only"; }
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
class Path;
class DataSource;

#if NODEL_ARCH == 32
using refcnt_t = uint32_t;                     // only least-significant 28-bits used
#else
using refcnt_t = uint64_t;                     // only least-significant 56-bits used
#endif

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

using StringPtr = IRCString*;
using ListPtr = IRCList*;
using MapPtr = IRCMap*;
using DataSourcePtr = DataSource*;

constexpr size_t min_key_chunk_size = 128;


// NOTE: use by-value semantics
class Object
{
  public:
    template <class KeyType> class KeyRange;
    template <class KeyType> class KeyIterator;

    template <class ValueType> class AncestorRange;
    template <class ValueType> class ChildrenRange;
    template <class ValueType> class SiblingRange;
    template <class ValueType> class DescendantRange;

    // TODO: remove _I suffix
    enum ReprType {
        EMPTY_I,  // uninitialized
        NULL_I,   // json null
        BOOL_I,
        INT_I,
        UINT_I,
        FLOAT_I,
        STR_I,
        LIST_I,
        OMAP_I,   // ordered map
        UMAP_I,   // TODO unordered map
        RBMAP_I,  // TODO red-black tree
        TABLE_I,  // TODO map-like key access, all rows have same keys (ex: CSV)
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
          Repr(StringPtr p)     : ps{p} {}
          Repr(ListPtr p)       : pl{p} {}
          Repr(MapPtr p)        : pm{p} {}
          Repr(DataSourcePtr p) : ds{p} {}

          void*         z;
          bool          b;
          Int           i;
          UInt          u;
          Float         f;
          StringPtr     ps;
          ListPtr       pl;
          MapPtr        pm;
          DataSourcePtr ds;
      };

      struct Fields
      {
          Fields(uint8_t repr_ix) : repr_ix{repr_ix}, ref_count{1} {}
          Fields(uint8_t repr_ix, refcnt_t ref_count) : repr_ix{repr_ix}, ref_count{ref_count} {}

          Fields(const Fields&) = delete;
          Fields(Fields&&) = delete;
          auto operator = (const Fields&) = delete;
          auto operator = (Fields&&) = delete;

#if NODEL_ARCH == 32
          uint8_t repr_ix:5;
          uint32_t ref_count:27;
#else
          uint8_t repr_ix:8;
          uint64_t ref_count:56;
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
    template <> bool val_conv<bool>() const     { return repr.b; }
    template <> Int val_conv<Int>() const       { return repr.i; }
    template <> UInt val_conv<UInt>() const     { return repr.u; }
    template <> Float val_conv<Float>() const   { return repr.f; }

    template <typename T> const T& val_conv() const requires std::is_same<T, String>::value {
        return std::get<0>(*repr.ps);
    }

  private:
    struct NoParent {};
    Object(const NoParent&) : repr{}, fields{NULL_I, 1} {}  // initialize reference count
    Object(NoParent&&)      : repr{}, fields{NULL_I, 1} {}  // initialize reference count

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
          case DSRC_I:  return "dsobj";
          default:      return "<undefined>";
      }
    }

  public:
    static constexpr refcnt_t no_ref_count = std::numeric_limits<refcnt_t>::max();

    Object()                           : repr{}, fields{EMPTY_I} {}
    Object(const std::string& str)     : repr{new IRCString{str, {}}}, fields{STR_I} {}
    Object(std::string&& str)          : repr{new IRCString(std::move(str), {})}, fields{STR_I} {}
    Object(const std::string_view& sv) : repr{new IRCString{{sv.data(), sv.size()}, {}}}, fields{STR_I} {}
    Object(const char* v)              : repr{new IRCString{v, {}}}, fields{STR_I} { assert(v != nullptr); }
    Object(bool v)                     : repr{v}, fields{BOOL_I} {}
    Object(is_like_Float auto v)       : repr{(Float)v}, fields{FLOAT_I} {}
    Object(is_like_Int auto v)         : repr{(Int)v}, fields{INT_I} {}
    Object(is_like_UInt auto v)        : repr{(UInt)v}, fields{UINT_I} {}
    Object(DataSourcePtr ptr)          : repr{ptr}, fields{DSRC_I} {}  // ownership transferred

    Object(List&& list);
    Object(Map&& map);

    Object(const Key& key);
    Object(Key&& key);

    Object(ReprType type);

    Object(const Object& other);
    Object(Object&& other);

    ~Object() { dec_ref_count(); }

    ReprType type() const;

    Object root() const;
    Object parent() const;

    AncestorRange<Object> iter_ancestors() const;
    ChildrenRange<Object> iter_children() const;
    SiblingRange<Object> iter_siblings() const;
    DescendantRange<Object> iter_descendants() const;

    // whole-tree const-ness
//    AncestorRange<const Object> iter_ancestors() const;
//    ChildrenRange<const Object> iter_children() const;
//    SiblingRange<const Object> iter_siblings() const;
//    DescendantRange<const Object> iter_descendants() const;

//    AncestorRange<Object> iter_ancestors();
//    ChildrenRange<Object> iter_children();
//    SiblingRange<Object> iter_siblings();
//    DescendantRange<Object> iter_descendants();

    template <typename Visitor>
    void iter_visit(Visitor&&) const;

    KeyRange<const Key> iter_keys() const;
    KeyRange<Key> iter_keys();

    const KeyList keys() const;
    const ItemList items() const;
    const List children() const;

    size_t size() const;

    const Key key() const;
    const Key key_of(const Object&) const;
    Path path() const;

    template <typename T>
    T value_cast() const;

    template <typename T>
    bool is_type() const { return resolve_repr_ix() == ix_conv<T>(); }

    template <typename V>
    void visit(V&& visitor) const;

//    template <typename T>
//    T as() requires is_byvalue<T> {
//        if (fields.repr_ix == ix_conv<T>()) return val_conv<T>();
//        else if (fields.repr_ix == DSRC_I) return dsrc_read().as<T>();
//        throw wrong_type(fields.repr_ix);
//    }
//
    template <typename T>
    const T as() const requires is_byvalue<T> {
        if (fields.repr_ix == ix_conv<T>()) return val_conv<T>();
        else if (fields.repr_ix == DSRC_I) return dsrc_read().as<T>();
        throw wrong_type(fields.repr_ix);
    }

//    template <typename T>
//    T& as() requires std::is_same<T, String>::value {
//        if (fields.repr_ix == ix_conv<T>()) return val_conv<T>();
//        else if (fields.repr_ix == DSRC_I) return dsrc_read().as<T>();
//        throw wrong_type(fields.repr_ix);
//    }

    template <typename T>
    const T& as() const requires std::is_same<T, String>::value {
        if (fields.repr_ix == ix_conv<T>()) return val_conv<T>();
        else if (fields.repr_ix == DSRC_I) return dsrc_read().as<T>();
        throw wrong_type(fields.repr_ix);
    }

    bool is_empty() const     { return fields.repr_ix == EMPTY_I; }
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

    bool to_bool() const;
    Int to_int() const;
    UInt to_uint() const;
    Float to_float() const;
    std::string to_str() const;
    Key to_key() const;
    Key to_tmp_key() const;  // returned key is a *view* with lifetime of this
    Key into_key();
    std::string to_json() const;

    const Object operator [] (is_byvalue auto) const;
    const Object operator [] (const char*) const;
    const Object operator [] (const std::string_view& v) const;

    Object operator [] (is_byvalue auto);
    Object operator [] (const char*);
    Object operator [] (const std::string_view& v);

    Object get(is_integral auto v) const;
    Object get(const char* v) const;
    Object get(const std::string_view& v) const;
    Object get(const std::string& v) const;
    Object get(bool v) const;
    Object get(const Key& key) const;
    Object get(const Object& obj) const;
    Object get(const Path& path) const;

    void set(is_integral auto, const Object&);
    void set(const char*, const Object&);
    void set(const std::string_view&, const Object&);
    void set(const std::string&, const Object&);
    void set(bool, const Object&);
    void set(const Key&, const Object&);
    void set(const Object&, const Object&);
    void set(std::pair<const Key&, const Object&>);
//    void set(const Path&, const Object&);

    void del(is_integral auto v);
    void del(const char* v);
    void del(const std::string_view& v);
    void del(const std::string& v);
    void del(bool v);
    void del(const Key& key);
    void del(const Object& key);
    void del(const Path& path);

    bool operator == (const Object&) const;
    std::partial_ordering operator <=> (const Object&) const;

    Oid id() const;
    bool is(const Object& other) const;
    refcnt_t ref_count() const;
    void inc_ref_count() const;
    void dec_ref_count() const;
    void release();
    void refer_to(const Object&);

    Object& operator = (const Object& other);
    Object& operator = (Object&& other);
    // TODO: Need other assignment ops to avoid Object temporaries

    bool has_data_source() const { return fields.repr_ix == DSRC_I; }

    template <class DataSourceType>
    DataSourceType& data_source() const {
        switch (fields.repr_ix) {
            case DSRC_I: return *dynamic_cast<DataSourceType*>(repr.ds);
            default: throw wrong_type(fields.repr_ix);
        }
    }

    void save();
    void reset();
    void reset_key(const Key&);
    void refresh();
    void refresh_key(const Key&);

  private:
    int resolve_repr_ix() const;
    Object dsrc_read() const;

    void set_parent(const Object& parent);
    void set_children_parent(IRCList& irc_list);
    void set_children_parent(IRCMap& irc_map);
    void clear_children_parent(IRCList& irc_list);
    void clear_children_parent(IRCMap& irc_map);

    static WrongType wrong_type(uint8_t actual)                   { return type_name(actual); };
    static WrongType wrong_type(uint8_t actual, uint8_t expected) { return {type_name(actual), type_name(expected)}; };
    static EmptyReference empty_reference(const char* func_name)  { return func_name; }

  private:
    Repr repr;
    Fields fields;

  friend class DataSource;
  friend class WalkDF;
  friend class WalkBF;
  template <class ValueType> friend class AncestorIterator;
  template <class ValueType> friend class ChildrenIterator;
  template <class ValueType> friend class SiblingIterator;
  template <class ValueType> friend class DescendantIterator;
};

constexpr Object::ReprType null = Object::NULL_I;


class Path
{
  public:
    Path() {}
    Path(const KeyList& keys)              : keys{keys} {}
    Path(KeyList&& keys)                   : keys{std::forward<KeyList>(keys)} {}
    Path(const Key& key)                   : keys{} { append(key); }
    Path(Key&& key)                        : keys{} { append(std::forward<Key>(key)); }

    void append(const Key& key)  { keys.push_back(key); }
    void append(Key&& key)       { keys.emplace_back(std::forward<Key>(key)); }
    void prepend(const Key& key) { keys.insert(keys.begin(), key); }
    void prepend(Key&& key)      { keys.emplace(keys.begin(), std::forward<Key>(key)); }

    Object lookup(const Object& origin) const {
        Object obj = origin;
        for (auto& key : keys)
            obj.refer_to(obj.get(key));
        return obj;
    }

    Object lookup_parent(const Object& origin) const {
        if (keys.size() == 0) return origin.parent();
        Object obj = origin;
        auto it = keys.begin();
        auto end = it + keys.size() - 1;
        for (; it != end; ++it)
            obj.refer_to(obj.get(*it));
        return obj;
    }

    std::string to_str() const {
        std::stringstream ss;
        for (auto& key : keys)
            key.to_step(ss);
        return ss.str();
    }

  private:
    KeyList keys;

  friend class Object;
};


inline
Path Object::path() const {
    Path path;
    Object obj = *this;
    Object par = parent();
    while (!par.is_null()) {
        path.prepend(par.key_of(obj));
        obj.refer_to(par);
        par.refer_to(obj.parent());
    }
    return path;
}


class DataSource
{
  public:
    template <class ChunkType>
    class ChunkIterator
    {
      public:
        virtual ~ChunkIterator() = default;
        virtual ChunkType& next() = 0;
        virtual bool destroy() = 0;
    };

    template <class ChunkType, typename ValueType>
    class Iterator
    {
      using IterType = std::conditional<std::is_const<ValueType>::value,
                                        typename ChunkType::const_iterator,
                                        typename ChunkType::iterator>::type;

      public:
        Iterator() = default;
        Iterator(ChunkIterator<ChunkType>* p_chit) : p_chit{p_chit} { next(); }
        ~Iterator() {
            if (p_chit != nullptr && p_chit->destroy()) delete p_chit;
        }

        Iterator(Iterator&& other) : p_chit{other.p_chit} { other.p_chit = nullptr; }
        auto& operator = (Iterator&& other) { p_chit = other.p_chit; other.p_chit = nullptr; return *this; }

        auto& operator ++ ()                     { if (++it == end) next(); return *this; }
        auto operator * () const                 { return *it; }
        bool operator == (const Iterator&) const { return it == end; }
        bool operator == (auto&&) const          { return it == end; }
        bool done() const                        { return it == end; }

        bool is_valid() const { return p_chit != nullptr; }

      private:
        void next() {
            auto& chunk = p_chit->next();
            it = chunk.begin();
            end = chunk.end();
        }

      private:
        ChunkIterator<ChunkType>* p_chit = nullptr;
        IterType it;
        IterType end;
    };

    template <class ChunkType, typename ValueType>
    class Range
    {
      public:
        using ChunkIterator = DataSource::ChunkIterator<ChunkType>;
        constexpr static int sentinel = 0;

        Range(ChunkIterator* p_chit) : p_chit{p_chit} {}
        Iterator<ChunkType, const ValueType> begin() const { return p_chit; }
        Iterator<ChunkType, ValueType> begin()             { return p_chit; }
        int end() const { return sentinel; }

      private:
        ChunkIterator* p_chit;
    };

    using ConstStringIterator = Iterator<String, const char>;
    using ConstKeyIterator = Iterator<KeyList, const Key>;
    using ConstValueIterator = Iterator<List, const Object>;
    using ConstItemIterator = Iterator<ItemList, const Item>;

    using StringIterator = Iterator<String, char>;
    using KeyIterator = Iterator<KeyList, Key>;
    using ValueIterator = Iterator<List, Object>;
    using ItemIterator = Iterator<ItemList, Item>;

    using ConstStringRange = Range<String, const char>;
    using ConstKeyRange = Range<KeyList, const Key>;
    using ConstValueRange = Range<List, const Object>;
    using ConstItemRange = Range<ItemList, ConstItem>;

    using StringRange = Range<String, char>;
    using KeyRange = Range<KeyList, Key>;
    using ValueRange = Range<List, Object>;
    using ItemRange = Range<ItemList, Item>;

  public:
    using ReprType = Object::ReprType;

    enum class Sparse { no = false, yes = true };
    enum class Mode   { ro = false, rw = true };

    DataSource(Sparse sparse, Mode mode) : sparse{sparse}, mode{mode} {}
    virtual ~DataSource() {}

    bool is_sparse() const { return sparse == Sparse::yes; }

  protected:
    virtual void read_meta(Object&) = 0;    // load meta-data including type and id
    virtual void read(Object& cache) = 0;
    virtual Object read_key(const Key&)               { return {}; }  // sparse
    virtual size_t read_size()                        { return 0; }  // sparse
    virtual void write(const Object&)                 {}  // both
    virtual void write_key(const Key&, const Object&) {}  // sparse
    virtual void write_key(const Key&, Object&&)      {}  // sparse

  public:
    virtual ChunkIterator<String>* str_iter()    { return nullptr; }  // sparse
    virtual ChunkIterator<KeyList>* key_iter()   { return nullptr; }  // sparse
    virtual ChunkIterator<List>* value_iter()    { return nullptr; }  // sparse
    virtual ChunkIterator<ItemList>* item_iter() { return nullptr; }  // sparse

    virtual std::string to_str() {
        insure_fully_cached();
        return cache.to_str();
    }

    const Object& get_cached() const { const_cast<DataSource*>(this)->insure_fully_cached(); return cache; }
    Object& get_cached()             { insure_fully_cached(); return cache; }

    size_t size() {
        if (is_sparse()) {
            return read_size();
        } else {
            insure_fully_cached();
            return cache.size();
        }
    }

    Object get(const Key& key) {
        if (is_sparse()) {
            if (cache.is_empty()) { read_meta(cache); }

            // empty value means "not cached" and null value means "cached, but no value"
            Object value = cache.get(key);
            if (value.is_empty()) {
                value = read_key(key);
                cache.set(key, value);
            }

            return value;
        } else {
            insure_fully_cached();
            return cache.get(key);
        }
    }

    void set(const Object& value) {
        if (mode == Mode::ro) throw ReadOnly();
        cache = value;
        fully_cached = true;
    }

    void set(const Key& key, const Object& value) {
        if (mode == Mode::ro) throw ReadOnly();
        if (is_sparse()) {
            if (cache.is_empty()) read_meta(cache);
            cache.set(key, value);
        } else {
            insure_fully_cached();
            cache.set(key, value);
        }
    }

    void del(const Key& key) {
        if (mode == Mode::ro) throw ReadOnly();
        if (is_sparse()) {
            if (cache.is_empty()) read_meta(cache);
            cache.set(key, null);
        } else {
            insure_fully_cached();
            cache.del(key);
        }
    }

    void save() {
        if (cache.is_empty()) return;
        if (fully_cached) {
            write(cache);
        } else if (is_sparse()) {
            for (auto& [key, value] : cache.items()) {
                write_key(key, value);
                if (key.is_null()) cache.del(key); // clear delete record
            }
        }
    }

    const Key key_of(const Object& obj) { return cache.key_of(obj); }

    ReprType type() { if (cache.is_empty()) read_meta(cache); return cache.type(); }
    Oid id()        { if (cache.is_empty()) read_meta(cache); return cache.id(); }

    void reset() {
        fully_cached = false;
        cache.release();
    }

    void reset_key(const Key& key) {
        if (is_sparse()) {
            if (!cache.is_empty())
                cache.del(key);
        } else {
            reset();
        }
    }

    void refresh()                   {}  // TODO: implement

    void refresh_key(const Key& key) {}  // TODO: implement

  protected:
    void insure_fully_cached() {
        if (!fully_cached) {
            read(cache);
            fully_cached = true;
        }
    }

  protected:
    Object cache;

  private:
    Object get_parent() const           { return parent; }
    void set_parent(Object& new_parent) { parent.refer_to(new_parent); }

    refcnt_t ref_count() const { return parent.fields.ref_count; }
    void inc_ref_count()       { parent.fields.ref_count++; }
    refcnt_t dec_ref_count()   { return --(parent.fields.ref_count); }

  private:
    Object parent;
    Sparse sparse;
    Mode mode;
    bool fully_cached = false;

  friend class Object;
};


template <typename KeyType>
class Object::KeyIterator
{
  public:
    using MapIterator = std::conditional<std::is_const<KeyType>::value,
                                         Map::const_iterator,
                                         Map::iterator>::type;

    using ChunkIterator = DataSource::ChunkIterator<KeyList>;
    using DataSourceIterator = DataSource::Iterator<KeyList, KeyType>;

    KeyIterator(const Map& map) {
        if constexpr (std::is_const<KeyType>::value) {
            map_it = map.cbegin();
            map_end = map.cend();
        } else {
            map_it = map.begin();
            map_end = map.end();
        }
    }

    KeyIterator(ChunkIterator* p_chit) : ds_it{p_chit} {}
    KeyIterator(KeyIterator&&) = default;

    KeyIterator& operator ++ () {
        if (ds_it.is_valid()) ++ds_it; else ++map_it;
        return *this;
    }

    KeyType operator * () const {
        return (ds_it.is_valid())? *ds_it: (*map_it).first;
    }

    bool operator == (auto&&) const {
        return ds_it.is_valid()? ds_it.done(): (map_it == map_end);
    }

  private:
    MapIterator map_it;
    MapIterator map_end;
    DataSourceIterator ds_it;
};

template <class KeyType>
class Object::KeyRange
{
  public:
    constexpr static auto sentinel = 0;

  public:
    KeyRange(const Object& obj) : obj{obj} {
        if (obj.fields.repr_ix == DSRC_I)
            p_chit = obj.repr.ds->key_iter();
    }

    ~KeyRange() {
        if (p_chit != nullptr && p_chit->destroy())
            delete p_chit;
    }

    KeyIterator<KeyType> begin() {
        switch (obj.fields.repr_ix) {
            case OMAP_I: return std::get<0>(*obj.repr.pm);
            case DSRC_I: return p_chit;
            default:     throw wrong_type(obj.fields.repr_ix);
        }
    }

    auto end() { return sentinel; }

  private:
    const Object& obj;
    DataSource::ChunkIterator<KeyList>* p_chit = nullptr;
};


inline
Object::Object(const Object& other) : fields{other.fields.repr_ix} {
//        fmt::print("Object(const Object& other)\n");
    switch (fields.repr_ix) {
        case EMPTY_I: repr.z = nullptr; break;
        case NULL_I:  repr.z = nullptr; break;
        case BOOL_I:  repr.b = other.repr.b; break;
        case INT_I:   repr.i = other.repr.i; break;
        case UINT_I:  repr.u = other.repr.u; break;
        case FLOAT_I: repr.f = other.repr.f; break;
        case STR_I:   other.inc_ref_count(); repr.ps = other.repr.ps; break;
        case LIST_I:  other.inc_ref_count(); repr.pl = other.repr.pl; break;
        case OMAP_I:  other.inc_ref_count(); repr.pm = other.repr.pm; break;
        case DSRC_I: repr.ds = other.repr.ds; repr.ds->inc_ref_count(); break;
    }
}

inline
Object::Object(Object&& other) : fields{other.fields.repr_ix} {
//        fmt::print("Object(Object&& other)\n");
    switch (fields.repr_ix) {
        case EMPTY_I: repr.z = nullptr; break;
        case NULL_I:  repr.z = nullptr; break;
        case BOOL_I:  repr.b = other.repr.b; break;
        case INT_I:   repr.i = other.repr.i; break;
        case UINT_I:  repr.u = other.repr.u; break;
        case FLOAT_I: repr.f = other.repr.f; break;
        case STR_I:   repr.ps = other.repr.ps; break;
        case LIST_I:  repr.pl = other.repr.pl; break;
        case OMAP_I:  repr.pm = other.repr.pm; break;
        case DSRC_I:  repr.ds = other.repr.ds; break;
    }

    other.fields.repr_ix = EMPTY_I;
    other.repr.z = nullptr;
}

inline
Object::Object(List&& list) : repr{new IRCList(std::move(list), NoParent{})}, fields{LIST_I} {
    List& my_list = std::get<0>(*repr.pl);
    for (auto& obj : my_list)
        obj.set_parent(*this);
}

inline
Object::Object(Map&& map) : repr{new IRCMap(std::move(map), NoParent{})}, fields{OMAP_I} {
    Map& my_map = std::get<0>(*repr.pm);
    for (auto& [key, obj]: my_map)
        const_cast<Object&>(obj).set_parent(*this);
}

inline
Object::Object(const Key& key) : fields{EMPTY_I} {
    switch (key.repr_ix) {
        case Key::NULL_I:  fields.repr_ix = NULL_I; repr.z = nullptr; break;
        case Key::BOOL_I:  fields.repr_ix = BOOL_I; repr.b = key.repr.b; break;
        case Key::INT_I:   fields.repr_ix = INT_I; repr.i = key.repr.i; break;
        case Key::UINT_I:  fields.repr_ix = UINT_I; repr.u = key.repr.u; break;
        case Key::FLOAT_I: fields.repr_ix = FLOAT_I; repr.f = key.repr.f; break;
        case Key::STR_I: {
            fields.repr_ix = STR_I;
            repr.ps = new IRCString{*key.repr.p_s, {}};
            break;
        }
        case Key::STRV_I: {
            fields.repr_ix = STR_I;
            auto& sv = key.repr.sv;
            repr.ps = new IRCString{std::string{sv.data(), sv.size()}, {}};
            break;
        }
        default: throw std::invalid_argument("key");
    }
}

inline
Object::Object(Key&& key) : fields{EMPTY_I} {
    switch (key.repr_ix) {
        case Key::NULL_I:  fields.repr_ix = NULL_I; repr.z = nullptr; break;
        case Key::BOOL_I:  fields.repr_ix = BOOL_I; repr.b = key.repr.b; break;
        case Key::INT_I:   fields.repr_ix = INT_I; repr.i = key.repr.i; break;
        case Key::UINT_I:  fields.repr_ix = UINT_I; repr.u = key.repr.u; break;
        case Key::FLOAT_I: fields.repr_ix = FLOAT_I; repr.f = key.repr.f; break;
        case Key::STR_I: {
            fields.repr_ix = STR_I;
            repr.ps = new IRCString{std::move(*key.repr.p_s), {}};
            break;
        }
        case Key::STRV_I: {
            fields.repr_ix = STR_I;
            auto& sv = key.repr.sv;
            repr.ps = new IRCString{std::string{sv.data(), sv.size()}, {}};
            break;
        }
        default: throw std::invalid_argument("key");
    }
}

inline
Object::Object(ReprType type) : fields{(uint8_t)type} {
    switch (type) {
        case EMPTY_I: repr.z = nullptr; break;
        case NULL_I:  repr.z = nullptr; break;
        case BOOL_I:  repr.b = false; break;
        case INT_I:   repr.i = 0; break;
        case UINT_I:  repr.u = 0; break;
        case FLOAT_I: repr.f = 0.0; break;
        case STR_I:   repr.ps = new IRCString{"", {}}; break;
        case LIST_I:  repr.pl = new IRCList{{}, {}}; break;
        case OMAP_I:  repr.pm = new IRCMap{{}, {}}; break;
        default:      throw wrong_type(type);
    }
}

inline
Object::ReprType Object::type() const {
    switch (fields.repr_ix) {
        case DSRC_I: return repr.ds->type();
        default:     return (ReprType)fields.repr_ix;
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
    switch (fields.repr_ix) {
        case EMPTY_I: throw empty_reference(__FUNCTION__);
        case STR_I:   return std::get<1>(*repr.ps);
        case LIST_I:  return std::get<1>(*repr.pl);
        case OMAP_I:  return std::get<1>(*repr.pm);
        case DSRC_I:  return repr.ds->get_parent();
        default:      return null;
    }
}

template <typename V>
void Object::visit(V&& visitor) const {
    switch (fields.repr_ix) {
        case EMPTY_I: throw empty_reference(__FUNCTION__);
        case NULL_I:  visitor(nullptr); break;
        case BOOL_I:  visitor(repr.b); break;
        case INT_I:   visitor(repr.i); break;
        case UINT_I:  visitor(repr.u); break;
        case FLOAT_I: visitor(repr.f); break;
        case STR_I:   visitor(std::get<0>(*repr.ps)); break;
        case DSRC_I:  visitor(*this); break;
        default:      throw wrong_type(fields.repr_ix);
    }
}

inline
int Object::resolve_repr_ix() const {
    switch (fields.repr_ix) {
        case DSRC_I: return const_cast<DataSourcePtr>(repr.ds)->type();
        default:     return fields.repr_ix;
    }
}

inline
Object Object::dsrc_read() const {
    return const_cast<DataSourcePtr>(repr.ds)->get_cached();
}

inline
void Object::refer_to(const Object& object) {
    release();
    (*this) = object;
}

inline
void Object::set_parent(const Object& new_parent) {
    switch (fields.repr_ix) {
        case STR_I: {
            auto& parent = std::get<1>(*repr.ps);
            parent.refer_to(new_parent);
            break;
        }
        case LIST_I: {
            auto& parent = std::get<1>(*repr.pl);
            parent.refer_to(new_parent);
            break;
        }
        case OMAP_I: {
            auto& parent = std::get<1>(*repr.pm);
            parent.refer_to(new_parent);
            break;
        }
        case DSRC_I: {
            auto parent = repr.ds->get_parent();
            parent.refer_to(new_parent);
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
    switch (fields.repr_ix) {
        case NULL_I: break;
        case LIST_I: {
            Oid oid = obj.id();
            const auto& list = std::get<0>(*repr.pl);
            UInt index = 0;
            for (const auto& item : list) {
                if (item.id() == oid)
                    return Key{index};
                index++;
            }
            break;
        }
        case OMAP_I: {
            Oid oid = obj.id();
            const auto& map = std::get<0>(*repr.pm);
            for (auto& [key, value] : map) {
                if (value.id() == oid)
                    return key;
            }
            break;
        }
        case DSRC_I: {
            return repr.ds->key_of(obj);
        }
        default:
            throw wrong_type(fields.repr_ix);
    }
    return Key{};
}

template <typename T>
T Object::value_cast() const {
    switch (fields.repr_ix) {
        case BOOL_I:  return (T)repr.b;
        case INT_I:   return (T)repr.i;
        case UINT_I:  return (T)repr.u;
        case FLOAT_I: return (T)repr.f;
        case DSRC_I:  return repr.ds->get_cached().value_cast<T>();
        default:      throw wrong_type(fields.repr_ix);
    }
}

inline
std::string Object::to_str() const {
    switch (fields.repr_ix) {
        case EMPTY_I: throw empty_reference(__FUNCTION__);
        case NULL_I:  return "null";
        case BOOL_I:  return repr.b? "true": "false";
        case INT_I:   return int_to_str(repr.i);
        case UINT_I:  return int_to_str(repr.u);
        case FLOAT_I: return nodel::float_to_str(repr.f);
        case STR_I:   return std::get<0>(*repr.ps);
        case LIST_I:
        case OMAP_I:  return to_json();
        case DSRC_I:  return const_cast<DataSourcePtr>(repr.ds)->to_str();
        default:
            throw wrong_type(fields.repr_ix);
    }
}

inline
bool Object::to_bool() const {
    switch (fields.repr_ix) {
        case EMPTY_I: throw empty_reference(__FUNCTION__);
        case NULL_I:  throw wrong_type(fields.repr_ix, BOOL_I);
        case BOOL_I:  return repr.b;
        case INT_I:   return repr.i;
        case UINT_I:  return repr.u;
        case FLOAT_I: return repr.f;
        case STR_I:   return str_to_bool(std::get<0>(*repr.ps));
        case DSRC_I:  return const_cast<DataSourcePtr>(repr.ds)->get_cached().to_bool();
        default:      throw wrong_type(fields.repr_ix, BOOL_I);
    }
}

inline
Int Object::to_int() const {
    switch (fields.repr_ix) {
        case EMPTY_I: throw empty_reference(__FUNCTION__);
        case NULL_I:  throw wrong_type(fields.repr_ix, INT_I);
        case BOOL_I:  return repr.b;
        case INT_I:   return repr.i;
        case UINT_I:  return repr.u;
        case FLOAT_I: return repr.f;
        case STR_I:   return str_to_int(std::get<0>(*repr.ps));
        case DSRC_I:  return const_cast<DataSourcePtr>(repr.ds)->get_cached().to_int();
        default:      throw wrong_type(fields.repr_ix, INT_I);
    }
}

inline
UInt Object::to_uint() const {
    switch (fields.repr_ix) {
        case EMPTY_I: throw empty_reference(__FUNCTION__);
        case NULL_I:  throw wrong_type(fields.repr_ix, UINT_I);
        case BOOL_I:  return repr.b;
        case INT_I:   return repr.i;
        case UINT_I:  return repr.u;
        case FLOAT_I: return repr.f;
        case STR_I:   return str_to_int(std::get<0>(*repr.ps));
        case DSRC_I:  return const_cast<DataSourcePtr>(repr.ds)->get_cached().to_uint();
        default:      throw wrong_type(fields.repr_ix, UINT_I);
    }
}

inline
Float Object::to_float() const {
    switch (fields.repr_ix) {
        case EMPTY_I: throw empty_reference(__FUNCTION__);
        case NULL_I:  throw wrong_type(fields.repr_ix, BOOL_I);
        case BOOL_I:  return repr.b;
        case INT_I:   return repr.i;
        case UINT_I:  return repr.u;
        case FLOAT_I: return repr.f;
        case STR_I:   return str_to_float(std::get<0>(*repr.ps));
        case DSRC_I:  return const_cast<DataSourcePtr>(repr.ds)->get_cached().to_float();
        default:      throw wrong_type(fields.repr_ix, FLOAT_I);
    }
}

inline
Key Object::to_key() const {
    switch (fields.repr_ix) {
        case EMPTY_I: throw empty_reference(__FUNCTION__);
        case NULL_I:  return nullptr;
        case BOOL_I:  return repr.b;
        case INT_I:   return repr.i;
        case UINT_I:  return repr.u;
        case FLOAT_I: return repr.f;
        case STR_I:   return std::get<0>(*repr.ps);
        case DSRC_I:  return const_cast<DataSourcePtr>(repr.ds)->get_cached().to_key();
        default:      throw wrong_type(fields.repr_ix);
    }
}

inline
Key Object::to_tmp_key() const {
    switch (fields.repr_ix) {
        case EMPTY_I: throw empty_reference(__FUNCTION__);
        case NULL_I:  return nullptr;
        case BOOL_I:  return repr.b;
        case INT_I:   return repr.i;
        case UINT_I:  return repr.u;
        case FLOAT_I: return repr.f;
        case STR_I:   return (std::string_view)(std::get<0>(*repr.ps));
        case DSRC_I:  return const_cast<DataSourcePtr>(repr.ds)->get_cached().to_tmp_key();
        default:      throw wrong_type(fields.repr_ix);
    }
}

inline
Key Object::into_key() {
    switch (fields.repr_ix) {
        case EMPTY_I: throw empty_reference(__FUNCTION__);
        case NULL_I:  { return nullptr; }
        case BOOL_I:  { Key k{repr.b}; release(); return k; }
        case INT_I:   { Key k{repr.i}; release();  return k; }
        case UINT_I:  { Key k{repr.u}; release();  return k; }
        case FLOAT_I: { Key k{repr.f}; release();  return k; }
        case STR_I:   {
            Key k{std::move(std::get<0>(*repr.ps))};
            release();
            return k;
        }
        case DSRC_I: {
            Key k{repr.ds->get_cached().into_key()};
            release();
            return k;
        }
        default:
            throw wrong_type(fields.repr_ix);
    }
}

inline const Object Object::operator [] (is_byvalue auto v) const         { return get(v); }
inline const Object Object::operator [] (const char* v) const             { return get(v); }
inline const Object Object::operator [] (const std::string_view& v) const { return get(v); }

inline Object Object::operator [] (is_byvalue auto v)         { return get(v); }
inline Object Object::operator [] (const char* v)             { return get(v); }
inline Object Object::operator [] (const std::string_view& v) { return get(v); }

inline
Object Object::get(const char* v) const {
    switch (fields.repr_ix) {
        case EMPTY_I: throw empty_reference(__FUNCTION__);
        case OMAP_I: {
            auto& map = std::get<0>(*repr.pm);
            auto it = map.find(v);
            if (it == map.end()) return {};
            return it->second;
        }
        case DSRC_I:  return repr.ds->get(v);
        default:      throw wrong_type(fields.repr_ix);
    }
}

inline
Object Object::get(const std::string_view& v) const {
    switch (fields.repr_ix) {
        case EMPTY_I: throw empty_reference(__FUNCTION__);
        case OMAP_I: {
            auto& map = std::get<0>(*repr.pm);
            auto it = map.find(v);
            if (it == map.end()) return {};
            return it->second;
        }
        case DSRC_I:  return repr.ds->get(v);
        default:      throw wrong_type(fields.repr_ix);
    }
}

inline
Object Object::get(const std::string& v) const {
    switch (fields.repr_ix) {
        case EMPTY_I: throw empty_reference(__FUNCTION__);
        case OMAP_I: {
            auto& map = std::get<0>(*repr.pm);
            auto it = map.find((std::string_view)v);
            if (it == map.end()) return {};
            return it->second;
        }
        case DSRC_I:  return repr.ds->get(v);
        default:      throw wrong_type(fields.repr_ix);
    }
}

inline
Object Object::get(is_integral auto index) const {
    switch (fields.repr_ix) {
        case EMPTY_I: throw empty_reference(__FUNCTION__);
        case LIST_I: {
            auto& list = std::get<0>(*repr.pl);
            return (index < 0)? list[list.size() + index]: list[index];
        }
        case OMAP_I: {
            auto& map = std::get<0>(*repr.pm);
            auto it = map.find(index);
            if (it == map.end()) return {};
            return it->second;
        }
        case DSRC_I:  return repr.ds->get(index);
        default:      throw wrong_type(fields.repr_ix);
    }
}

inline
Object Object::get(bool v) const {
    switch (fields.repr_ix) {
        case EMPTY_I: throw empty_reference(__FUNCTION__);
        case OMAP_I: {
            auto& map = std::get<0>(*repr.pm);
            auto it = map.find(v);
            if (it == map.end()) return {};
            return it->second;
        }
        case DSRC_I:  return repr.ds->get(v);
        default:      throw wrong_type(fields.repr_ix);
    }
}

inline
Object Object::get(const Key& key) const {
    switch (fields.repr_ix) {
        case EMPTY_I: throw empty_reference(__FUNCTION__);
        case LIST_I: {
            auto& list = std::get<0>(*repr.pl);
            Int index = key.to_int();
            return (index < 0)? list[list.size() + index]: list[index];
        }
        case OMAP_I: {
            auto& map = std::get<0>(*repr.pm);
            auto it = map.find(key);
            if (it == map.end()) return {};
            return it->second;
        }
        case DSRC_I:  return repr.ds->get(key);
        default:
            throw wrong_type(fields.repr_ix);
    }
}

inline
Object Object::get(const Object& obj) const {
    switch (fields.repr_ix) {
        case EMPTY_I: throw empty_reference(__FUNCTION__);
        case LIST_I: {
            auto& list = std::get<0>(*repr.pl);
            Int index = obj.to_int();
            return (index < 0)? list[list.size() + index]: list[index];
        }
        case OMAP_I: {
            auto& map = std::get<0>(*repr.pm);
            auto it = map.find(obj.to_tmp_key());
            if (it == map.end()) return {};
            return it->second;
        }
        case DSRC_I:  return repr.ds->get(obj.to_tmp_key());
        default:      throw wrong_type(fields.repr_ix);
    }
}

inline
Object Object::get(const Path& path) const {
    return path.lookup(*this);
}

inline
void Object::set(is_integral auto key, const Object& value) { set(Key{key}, value); }

inline
void Object::set(const char* key, const Object& value) { set(Key{key}, value); }

inline
void Object::set(const std::string_view& key, const Object& value) { set(Key{key}, value); }

inline
void Object::set(const std::string& key, const Object& value) { set(Key{key}, value); }

inline
void Object::set(bool key, const Object& value) { set(Key{key}, value); }

inline
void Object::set(const Key& key, const Object& value) {
    switch (fields.repr_ix) {
        case EMPTY_I: throw empty_reference(__FUNCTION__);
        case LIST_I: {
            auto& list = std::get<0>(*repr.pl);
            Int index = key.to_int();
            auto size = list.size();
            if (index < 0) index += size;
            if (index > size) index = size;
            list[index] = value;
            break;
        }
        case OMAP_I: {
            auto& map = std::get<0>(*repr.pm);
            map.insert_or_assign(key, value);
            break;
        }
        case DSRC_I:  return repr.ds->set(key, value);
        default:
            throw wrong_type(fields.repr_ix);
    }
}

inline
void Object::set(const Object& key, const Object& value) {
    set(key.to_key(), value);
}

inline
void Object::set(std::pair<const Key&, const Object&> item) {
    set(item.first, item.second);
}

//inline
//void Object::set(const Path& path, const Object& value) {
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
void Object::del(const std::string_view& v) {
    del(Key{v});
}

inline
void Object::del(const std::string& v) {
    del(Key{(std::string_view)v});
}

inline
void Object::del(bool v) {
    del(Key{v});
}

inline
void Object::del(const Key& key) {
    switch (fields.repr_ix) {
        case EMPTY_I: throw empty_reference(__FUNCTION__);
        case LIST_I: {
            auto& list = std::get<0>(*repr.pl);
            Int index = key.to_int();
            auto size = list.size();
            if (index < 0) index += size;
            if (index > size) index = size;
            list.erase(list.begin() + index);
            break;
        }
        case OMAP_I: {
            auto& map = std::get<0>(*repr.pm);
            auto it = map.find(key);
            if (it != map.end()) {
                auto value = it->second;
                value.set_parent(null);
                map.erase(it);
            }
            break;
        }
        case DSRC_I: return repr.ds->del(key);
        default:
            throw wrong_type(fields.repr_ix);
    }
}

inline
void Object::del(const Object& key) {
    del(key.to_key());
}

inline
void Object::del(const Path& path) {
    path.lookup_parent(*this).del(key());
}

inline
size_t Object::size() const {
    switch (fields.repr_ix) {
        case EMPTY_I: throw empty_reference(__FUNCTION__);
        case STR_I:   return std::get<0>(*repr.ps).size();
        case LIST_I:  return std::get<0>(*repr.pl).size();
        case OMAP_I:  return std::get<0>(*repr.pm).size();
        case DSRC_I:  return repr.ds->size();
        default:
            return 0;
    }
}

// TODO: visit function constraints
template <typename VisitFunc>
void Object::iter_visit(VisitFunc&& visit) const {
    switch (fields.repr_ix) {
        case STR_I: {
            for (auto c : std::get<0>(*repr.ps)) {
                if (!visit(c))
                    break;
            }
            return;
        }
        case LIST_I: {
            for (const auto& obj : std::get<0>(*repr.pl)) {
                if (!visit(obj))
                    break;
            }
            return;
        }
        case OMAP_I: {
            auto& map = std::get<0>(*repr.pm);
            for (const auto& item : map) {
                if (!visit(std::get<0>(item)))
                    break;
            }
            return;
        }
        case DSRC_I: {
            auto& dsrc = *repr.ds;
            if (dsrc.is_sparse()) {
                auto type = dsrc.type();
                switch (type) {
                    case STR_I: {
                        DataSource::StringRange range{dsrc.str_iter()};
                        for (auto c : range)
                            if (!visit(c))
                                break;
                        return;
                    }
                    case LIST_I: {
                        DataSource::ValueRange range{dsrc.value_iter()};
                        for (auto val : range)
                            if (!visit(val))
                                break;
                        return;
                    }
                    case OMAP_I: {
                        DataSource::KeyRange range{dsrc.key_iter()};
                        for (auto val : range)
                            if (!visit(val))
                                break;
                        return;
                    }
                    default:
                        throw wrong_type(type);
                }
            } else {
                dsrc.get_cached().iter_visit(visit);
            }
            return;
        }
        default:
            throw wrong_type(fields.repr_ix);
    }
}

inline Object::KeyRange<const Key> Object::iter_keys() const { return *this; }
inline Object::KeyRange<Key> Object::iter_keys() { return *this; }

inline
const KeyList Object::keys() const {
    KeyList keys;
    for (auto key : iter_keys())
        keys.push_back(key);
    return keys;
}

inline
const List Object::children() const {
    List children;
    switch (fields.repr_ix) {
        case LIST_I:  {
            for (auto& child : std::get<0>(*repr.pl))
                children.push_back(child);
            break;
        }
        case OMAP_I:  {
            for (auto& item : std::get<0>(*repr.pm))
                children.push_back(item.second);
            break;
        }
        case DSRC_I: {
            DataSource::ConstValueRange range{repr.ds->value_iter()};
            for (const auto& value : range)
                children.push_back(value);
            break;
        }
        default:
            throw wrong_type(fields.repr_ix);
    }
    return children;
}

inline
const ItemList Object::items() const {
    ItemList items;
    switch (fields.repr_ix) {
        case OMAP_I:  {
            for (auto& item : std::get<0>(*repr.pm))
                items.push_back(item);
            break;
        }
        case DSRC_I: {
            DataSource::ConstItemRange range{repr.ds->item_iter()};
            for (const auto& item : range)
                items.push_back(item);
            break;
        }
        default:
            throw wrong_type(fields.repr_ix);
    }
    return items;
}

inline
bool Object::operator == (const Object& obj) const {
    if (is_empty() || obj.is_empty()) throw empty_reference(__FUNCTION__);
    if (is(obj)) return true;

    switch (fields.repr_ix) {
        case NULL_I: {
            if (obj.fields.repr_ix == NULL_I) return true;
            throw wrong_type(fields.repr_ix);
        }
        case BOOL_I: {
            switch (obj.fields.repr_ix)
            {
                case BOOL_I:  return repr.b == obj.repr.b;
                case INT_I:   return repr.b == obj.repr.i;
                case UINT_I:  return repr.b == obj.repr.u;
                case FLOAT_I: return repr.b == obj.repr.f;
                default:
                    throw wrong_type(fields.repr_ix);
            }
        }
        case INT_I: {
            switch (obj.fields.repr_ix)
            {
                case BOOL_I:  return repr.i == obj.repr.b;
                case INT_I:   return repr.i == obj.repr.i;
                case UINT_I:  return repr.i == obj.repr.u;
                case FLOAT_I: return repr.i == obj.repr.f;
                default:
                    throw wrong_type(fields.repr_ix);
            }
        }
        case UINT_I: {
            switch (obj.fields.repr_ix)
            {
                case BOOL_I:  return repr.u == obj.repr.b;
                case INT_I:   return repr.u == obj.repr.i;
                case UINT_I:  return repr.u == obj.repr.u;
                case FLOAT_I: return repr.u == obj.repr.f;
                default:
                    throw wrong_type(fields.repr_ix);
            }
        }
        case FLOAT_I: {
            switch (obj.fields.repr_ix)
            {
                case BOOL_I:  return repr.f == obj.repr.b;
                case INT_I:   return repr.f == obj.repr.i;
                case UINT_I:  return repr.f == obj.repr.u;
                case FLOAT_I: return repr.f == obj.repr.f;
                default:
                    throw wrong_type(fields.repr_ix);
            }
        }
        case STR_I: {
            if (obj.fields.repr_ix == STR_I) return std::get<0>(*repr.ps) == std::get<0>(*obj.repr.ps);
            throw wrong_type(fields.repr_ix);
        }
        case DSRC_I: return repr.ds->get_cached() == obj;
        default:     throw wrong_type(fields.repr_ix);
    }
}

inline
std::partial_ordering Object::operator <=> (const Object& obj) const {
    using ordering = std::partial_ordering;

    if (is_empty() || obj.is_empty()) throw empty_reference(__FUNCTION__);
    if (is(obj)) return ordering::equivalent;

    switch (fields.repr_ix) {
        case NULL_I: {
            if (obj.fields.repr_ix != NULL_I)
                throw wrong_type(fields.repr_ix);
            return ordering::equivalent;
        }
        case BOOL_I: {
            if (obj.fields.repr_ix != BOOL_I)
                throw wrong_type(fields.repr_ix);
            return repr.b <=> obj.repr.b;
        }
        case INT_I: {
            switch (obj.fields.repr_ix) {
                case INT_I:   return repr.i <=> obj.repr.i;
                case UINT_I:
                    if (obj.repr.u > std::numeric_limits<Int>::max()) return ordering::less;
                    return repr.i <=> (Int)obj.repr.u;
                case FLOAT_I: return repr.i <=> obj.repr.f;
                default:
                    throw wrong_type(fields.repr_ix);
            }
            break;
        }
        case UINT_I: {
            switch (obj.fields.repr_ix) {
                case INT_I:
                    if (repr.u > std::numeric_limits<Int>::max()) return ordering::greater;
                    return (Int)repr.u <=> obj.repr.i;
                case UINT_I:  return repr.u <=> obj.repr.u;
                case FLOAT_I: return repr.u <=> obj.repr.f;
                default:
                    throw wrong_type(fields.repr_ix);
            }
        }
        case FLOAT_I: {
            switch (obj.fields.repr_ix) {
                case INT_I:   return repr.f <=> obj.repr.i;
                case UINT_I:  return repr.f <=> obj.repr.u;
                case FLOAT_I: return repr.f <=> obj.repr.f;
                default:
                    throw wrong_type(fields.repr_ix);
            }
            break;
        }
        case STR_I: {
            if (obj.fields.repr_ix != STR_I)
                throw wrong_type(fields.repr_ix);
            return std::get<0>(*repr.ps) <=> std::get<0>(*obj.repr.ps);
        }
        case DSRC_I: return repr.ds->get_cached() <=> obj;
        default:     throw wrong_type(fields.repr_ix);
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
    : visitor{visitor}
    {
        if (root.is_empty()) throw root.empty_reference(__FUNCTION__);
        stack.emplace(Object(), 0, root, FIRST_VALUE);
    }

    bool next() {
        bool has_next = !stack.empty();
        if (has_next) {
            const auto [parent, key, object, event] = stack.top();
            stack.pop();

            if (event & END_PARENT) {
                visitor(parent, key, object, event);
            } else {
                switch (object.fields.repr_ix) {
                    case Object::LIST_I: {
                        visitor(parent, key, object, event | BEGIN_PARENT);
                        stack.emplace(parent, key, object, event | END_PARENT);
                        auto& list = std::get<0>(*object.repr.pl);
                        Int index = list.size() - 1;
                        for (auto it = list.crbegin(); it != list.crend(); it++, index--) {
                            stack.emplace(object, index, *it, (index == 0)? FIRST_VALUE: NEXT_VALUE);
                        }
                        break;
                    }
                    case Object::OMAP_I: {
                        visitor(parent, key, object, event | BEGIN_PARENT);
                        stack.emplace(parent, key, object, event | END_PARENT);
                        auto& map = std::get<0>(*object.repr.pm);
                        Int index = map.size() - 1;
                        for (auto it = map.rcbegin(); it != map.rcend(); it++, index--) {
                            auto& [key, child] = *it;
                            stack.emplace(object, key, child, (index == 0)? FIRST_VALUE: NEXT_VALUE);
                        }
                        break;
                    }
                    case Object::DSRC_I: {
                        stack.emplace(parent, key, object.repr.ds->get_cached(), FIRST_VALUE);
                        break;
                    }
                    default: {
                        visitor(parent, key, object, event);
                        break;
                    }
                }
            }
        }
        return has_next;
    }

private:
    Visitor visitor;
    Object root;
    Stack stack;
};


class WalkBF
{
public:
    using Item = std::tuple<Object, Key, Object>;
    using Deque = std::deque<Item>;
    using Visitor = std::function<void(const Object&, const Key&, const Object&)>;

public:
    WalkBF(Object root, Visitor visitor)
    : visitor{visitor}
    {
        if (root.is_empty()) throw root.empty_reference(__FUNCTION__);
        deque.emplace_back(Object(), 0, root);
    }

    bool next() {
        bool has_next = !deque.empty();
        if (has_next) {
            const auto [parent, key, object] = deque.front();
            deque.pop_front();

            switch (object.fields.repr_ix) {
                case Object::LIST_I: {
                    auto& list = std::get<0>(*object.repr.pl);
                    Int index = 0;
                    for (auto it = list.cbegin(); it != list.cend(); it++, index++) {
                        const auto& child = *it;
                        deque.emplace_back(object, index, child);
                    }
                    break;
                }
                case Object::OMAP_I: {
                    auto& map = std::get<0>(*object.repr.pm);
                    for (auto it = map.cbegin(); it != map.cend(); it++) {
                        auto& [key, child] = *it;
                        deque.emplace_back(object, key, child);
                    }
                    break;
                }
                case Object::DSRC_I: {
                    deque.emplace_front(parent, key, object.repr.ds->get_cached());
                    break;
                }
                default: {
                    visitor(parent, key, object);
                    break;
                }
            }
        }
        return has_next;
    }

private:
    Visitor visitor;
    Object root;
    Deque deque;
};


inline
std::string Object::to_json() const {
    std::stringstream ss;

    auto visitor = [&ss] (const Object& parent, const Key& key, const Object& object, uint8_t event) -> void {
        if (event & WalkDF::NEXT_VALUE && !(event & WalkDF::END_PARENT)) {
            ss << ", ";
        }

        if (parent.is_map() && !(event & WalkDF::END_PARENT)) {
            ss << key.to_json() << ": ";
        }

        switch (object.fields.repr_ix) {
            case EMPTY_I: throw empty_reference(__FUNCTION__);
            case NULL_I:  ss << "null"; break;
            case BOOL_I:  ss << (object.repr.b? "true": "false"); break;
            case INT_I:   ss << int_to_str(object.repr.i); break;
            case UINT_I:  ss << int_to_str(object.repr.u); break;
            case FLOAT_I: ss << float_to_str(object.repr.f); break;
            case STR_I:   ss << std::quoted(std::get<0>(*object.repr.ps)); break;
            case LIST_I:  ss << ((event & WalkDF::BEGIN_PARENT)? '[': ']'); break;
            case OMAP_I:  ss << ((event & WalkDF::BEGIN_PARENT)? '{': '}'); break;
            default:      throw wrong_type(object.fields.repr_ix);
        }
    };

    WalkDF walk{*this, visitor};
    while (walk.next());

    return ss.str();
}

inline
Oid Object::id() const {
    switch (fields.repr_ix) {
        case EMPTY_I: throw empty_reference(__FUNCTION__);
        case NULL_I:  return Oid::null();
        case BOOL_I:  return Oid{1, repr.b};
        case INT_I:   return Oid{2, *((uint64_t*)(&repr.i))};
        case UINT_I:  return Oid{3, repr.u};;
        case FLOAT_I: return Oid{4, *((uint64_t*)(&repr.f))};;
        case STR_I:   return Oid{5, (uint64_t)(&(std::get<0>(*repr.ps)))};
        case LIST_I:  return Oid{6, (uint64_t)(&(std::get<0>(*repr.pl)))};
        case OMAP_I:  return Oid{7, (uint64_t)(&(std::get<0>(*repr.pm)))};
        case DSRC_I:  return Oid{8, (uint64_t)(repr.ds)};
        default:      throw wrong_type(fields.repr_ix);
    }
}

inline
bool Object::is(const Object& other) const {
    auto repr_ix = fields.repr_ix;
    if (other.fields.repr_ix != repr_ix) return false;
    switch (repr_ix) {
        case EMPTY_I: throw empty_reference(__FUNCTION__);
        case NULL_I:  return true;
        case BOOL_I:  return repr.b == other.repr.b;
        case INT_I:   return repr.i == other.repr.i;
        case UINT_I:  return repr.u == other.repr.u;
        case FLOAT_I: return repr.f == other.repr.f;
        case STR_I:   return repr.ps == other.repr.ps;
        case LIST_I:  return repr.pl == other.repr.pl;
        case OMAP_I:  return repr.pm == other.repr.pm;
        case DSRC_I:  return repr.ds == other.repr.ds;
        default:      throw wrong_type(fields.repr_ix);
    }
}

inline
refcnt_t Object::ref_count() const {
    switch (fields.repr_ix) {
        case STR_I:   return std::get<1>(*repr.ps).fields.ref_count;
        case LIST_I:  return std::get<1>(*repr.pl).fields.ref_count;
        case OMAP_I:  return std::get<1>(*repr.pm).fields.ref_count;
        case DSRC_I:  return repr.ds->ref_count();
        default:      return no_ref_count;
    }
}


inline
void Object::inc_ref_count() const {
    Object& self = *const_cast<Object*>(this);
    switch (fields.repr_ix) {
        case EMPTY_I: throw empty_reference(__FUNCTION__);
        case STR_I:   ++(std::get<1>(*self.repr.ps).fields.ref_count); break;
        case LIST_I:  ++(std::get<1>(*self.repr.pl).fields.ref_count); break;
        case OMAP_I:  ++(std::get<1>(*self.repr.pm).fields.ref_count); break;
        case DSRC_I:  repr.ds->inc_ref_count(); break;
        default:      break;
    }
}

inline
void Object::dec_ref_count() const {
    Object& self = *const_cast<Object*>(this);
    switch (fields.repr_ix) {
        case STR_I:   if (--(std::get<1>(*self.repr.ps).fields.ref_count) == 0) delete self.repr.ps; break;
        case LIST_I:  if (--(std::get<1>(*self.repr.pl).fields.ref_count) == 0) delete self.repr.pl; break;
        case OMAP_I:  if (--(std::get<1>(*self.repr.pm).fields.ref_count) == 0) delete self.repr.pm; break;
        case DSRC_I:  if (repr.ds->dec_ref_count() == 0) delete self.repr.ds; break;
        default:      break;
    }
}

inline
void Object::release() {
    dec_ref_count();
    repr.z = nullptr;
    fields.repr_ix = EMPTY_I;
}

inline
void Object::set_children_parent(IRCList& irc_list) {
    Object& self = *this;
    auto& list = std::get<0>(irc_list);
    for (auto& obj : list)
        obj.set_parent(self);
}

inline
void Object::set_children_parent(IRCMap& irc_map) {
    auto& map = std::get<0>(irc_map);
    for (auto& [key, obj] : map)
        const_cast<Object&>(obj).set_parent(*this);
}

inline
void Object::clear_children_parent(IRCList& irc_list) {
    auto& list = std::get<0>(irc_list);
    for (auto& obj : list)
        obj.set_parent(null);
}

inline
void Object::clear_children_parent(IRCMap& irc_map) {
    auto& map = std::get<0>(irc_map);
    for (auto& [key, obj] : map)
        const_cast<Object&>(obj).set_parent(null);
}

inline
Object& Object::operator = (const Object& other) {
//    fmt::print("Object::operator = (const Object&)\n");
    if (fields.repr_ix == EMPTY_I)
    {
        fields.repr_ix = other.fields.repr_ix;
        switch (fields.repr_ix) {
            case EMPTY_I: throw empty_reference(__FUNCTION__);
            case NULL_I:  repr.z = nullptr; break;
            case BOOL_I:  repr.b = other.repr.b; break;
            case INT_I:   repr.i = other.repr.i; break;
            case UINT_I:  repr.u = other.repr.u; break;
            case FLOAT_I: repr.f = other.repr.f; break;
            case STR_I: {
                repr.ps = other.repr.ps;
                inc_ref_count();
                break;
            }
            case LIST_I: {
                repr.pl = other.repr.pl;
                inc_ref_count();
                break;
            }
            case OMAP_I: {
                repr.pm = other.repr.pm;
                inc_ref_count();
                break;
            }
            case DSRC_I: {
                repr.ds = other.repr.ds;
                inc_ref_count();
                break;
            }
        }
    }
    else if (fields.repr_ix == other.fields.repr_ix)
    {
        if (!is(other)) {
            switch (fields.repr_ix) {
                case EMPTY_I: throw empty_reference(__FUNCTION__);
                case NULL_I:  repr.z = nullptr; break;
                case BOOL_I:  repr.b = other.repr.b; break;
                case INT_I:   repr.i = other.repr.i; break;
                case UINT_I:  repr.u = other.repr.u; break;
                case FLOAT_I: repr.f = other.repr.f; break;
                case STR_I: {
                    std::get<0>(*repr.ps) = std::get<0>(*other.repr.ps);
                    break;
                }
                case LIST_I: {
                    clear_children_parent(*repr.pl);
                    std::get<0>(*repr.pl) = std::get<0>(*other.repr.pl);
                    set_children_parent(*repr.pl);
                    break;
                }
                case OMAP_I: {
                    clear_children_parent(*repr.pm);
                    std::get<0>(*repr.pm) = std::get<0>(*other.repr.pm);
                    set_children_parent(*repr.pm);
                    break;
                }
                case DSRC_I: {
                    repr.ds->set(other);
                    break;
                }
            }
        }
    }
    else
    {
        Object curr_parent{parent()};
        auto repr_ix = fields.repr_ix;

        switch (repr_ix) {
            case STR_I:  dec_ref_count(); break;
            case LIST_I: clear_children_parent(*repr.pl); dec_ref_count(); break;
            case OMAP_I: clear_children_parent(*repr.pm); dec_ref_count(); break;
            case DSRC_I: repr.ds->set(other); break;
            default:     break;
        }

        if (repr_ix != DSRC_I) {
            fields.repr_ix = other.fields.repr_ix;
            switch (fields.repr_ix) {
                case EMPTY_I: throw empty_reference(__FUNCTION__);
                case NULL_I:  repr.z = nullptr; break;
                case BOOL_I:  repr.b = other.repr.b; break;
                case INT_I:   repr.i = other.repr.i; break;
                case UINT_I:  repr.u = other.repr.u; break;
                case FLOAT_I: repr.f = other.repr.f; break;
                case STR_I: {
                    String& str = std::get<0>(*other.repr.ps);
                    repr.ps = new IRCString{str, curr_parent};
                    break;
                }
                case LIST_I: {
                    auto& list = std::get<0>(*other.repr.pl);
                    repr.pl = new IRCList{list, curr_parent};
                    set_children_parent(*repr.pl);
                    break;
                }
                case OMAP_I: {
                    auto& map = std::get<0>(*other.repr.pm);
                    repr.pm = new IRCMap{map, curr_parent};
                    set_children_parent(*repr.pm);
                    break;
                }
            }
        }
    }
    return *this;
}

inline
Object& Object::operator = (Object&& other) {
//    fmt::print("Object::operator = (Object&&)\n");
    if (fields.repr_ix == EMPTY_I)
    {
        fields.repr_ix = other.fields.repr_ix;
        switch (fields.repr_ix) {
            case EMPTY_I: throw empty_reference(__FUNCTION__);
            case NULL_I:  repr.z = nullptr; break;
            case BOOL_I:  repr.b = other.repr.b; break;
            case INT_I:   repr.i = other.repr.i; break;
            case UINT_I:  repr.u = other.repr.u; break;
            case FLOAT_I: repr.f = other.repr.f; break;
            case STR_I:   repr.ps = other.repr.ps; break;
            case LIST_I:  repr.pl = other.repr.pl; break;
            case OMAP_I:  repr.pm = other.repr.pm; break;
            case DSRC_I:  repr.ds = other.repr.ds; break;
        }
    }
    else if (fields.repr_ix == other.fields.repr_ix)
    {
        if (!is(other)) {
            switch (fields.repr_ix) {
                case EMPTY_I: throw empty_reference(__FUNCTION__);
                case NULL_I:  repr.z = nullptr; break;
                case BOOL_I:  repr.b = other.repr.b; break;
                case INT_I:   repr.i = other.repr.i; break;
                case UINT_I:  repr.u = other.repr.u; break;
                case FLOAT_I: repr.f = other.repr.f; break;
                case STR_I: {
                    std::get<0>(*repr.ps) = std::get<0>(*other.repr.ps);
                    break;
                }
                case LIST_I: {
                    clear_children_parent(*repr.pl);
                    std::get<0>(*repr.pl) = std::get<0>(*other.repr.pl);
                    set_children_parent(*repr.pl);
                    break;
                }
                case OMAP_I: {
                    clear_children_parent(*repr.pm);
                    std::get<0>(*repr.pm) = std::get<0>(*other.repr.pm);
                    set_children_parent(*repr.pm);
                    break;
                }
                case DSRC_I: repr.ds->set(other); break;
            }
        }
    }
    else
    {
        Object curr_parent{parent()};
        auto repr_ix = fields.repr_ix;

        switch (repr_ix) {
            case STR_I:   dec_ref_count(); break;
            case LIST_I:  clear_children_parent(*repr.pl); dec_ref_count(); break;
            case OMAP_I:  clear_children_parent(*repr.pm); dec_ref_count(); break;
            case DSRC_I:  repr.ds->set(other); break;
            default:     break;
        }

        if (repr_ix != DSRC_I) {
            fields.repr_ix = other.fields.repr_ix;
            switch (fields.repr_ix) {
                case EMPTY_I: throw empty_reference(__FUNCTION__);
                case NULL_I:  repr.z = nullptr; break;
                case BOOL_I:  repr.b = other.repr.b; break;
                case INT_I:   repr.i = other.repr.i; break;
                case UINT_I:  repr.u = other.repr.u; break;
                case FLOAT_I: repr.f = other.repr.f; break;
                case STR_I: {
                    String& str = std::get<0>(*other.repr.ps);
                    repr.ps = new IRCString{std::move(str), curr_parent};
                    break;
                }
                case LIST_I: {
                    auto& list = std::get<0>(*other.repr.pl);
                    repr.pl = new IRCList{std::move(list), curr_parent};
                    set_children_parent(*repr.pl);
                    break;
                }
                case OMAP_I: {
                    auto& map = std::get<0>(*other.repr.pm);
                    repr.pm = new IRCMap{std::move(map), curr_parent};
                    set_children_parent(*repr.pm);
                    break;
                }
            }
        }
    }

    other.fields.repr_ix = EMPTY_I;
    other.repr.z = nullptr; // safe, but may not be necessary

    return *this;
}

inline
void Object::save() {
    switch (fields.repr_ix) {
        case DSRC_I: repr.ds->save(); break;
        default:      break;
    }
}

inline
void Object::reset() {
    switch (fields.repr_ix) {
        case DSRC_I: repr.ds->reset(); break;
        default:      break;
    }
}

inline
void Object::reset_key(const Key& key) {
    switch (fields.repr_ix) {
        case DSRC_I: repr.ds->reset_key(key); break;
        default:      break;
    }
}

inline
void Object::refresh() {
    switch (fields.repr_ix) {
        case DSRC_I: repr.ds->refresh(); break;
        default:      break;
    }
}

inline
void Object::refresh_key(const Key& key) {
    switch (fields.repr_ix) {
        case DSRC_I: repr.ds->refresh_key(key); break;
        default:      break;
    }
}


template <typename ValueType>
class AncestorIterator
{
  public:
    AncestorIterator(const Object& object) : object{object} {}
    AncestorIterator(Object&& object) : object{std::forward<Object>(object)} {}

    AncestorIterator(const AncestorIterator&) = default;
    AncestorIterator(AncestorIterator&&) = default;

    AncestorIterator<ValueType>& operator ++ () {
        object.refer_to(object.parent());
        if (object.is_null()) object.release();
        return *this;
    }
    Object operator * () const { return object; }
    bool operator == (const AncestorIterator& other) const {
        if (object.is_empty()) return other.object.is_empty();
        return !other.object.is_empty() && object.id() == other.object.id();
    }
  private:
    Object object;
};

template <typename ValueType>
class Object::AncestorRange
{
  public:
    using AncestorIterator = AncestorIterator<ValueType>;

    AncestorRange(const Object& object) : object{object.parent()} {}
    AncestorIterator begin() const { return object; }
    AncestorIterator end() const   { return Object{}; }
  private:
    Object object;
};

inline Object::AncestorRange<Object> Object::iter_ancestors() const { return *this; }
//inline Object::AncestorRange<const Object> Object::iter_ancestors() const { return *this; }
//inline Object::AncestorRange<Object> Object::iter_ancestors() { return *this; }


template <typename ValueType>
class ChildrenIterator
{
  public:
    using List_iterator = std::conditional<std::is_const<ValueType>::value,
                                           List::const_iterator,
                                           List::iterator>::type;

    using Map_iterator = std::conditional<std::is_const<ValueType>::value,
                                          Map::const_iterator,
                                          Map::iterator>::type;

    ChildrenIterator(uint8_t repr_ix)                                          : repr_ix{repr_ix} {}
    ChildrenIterator(uint8_t repr_ix, const List_iterator& it)                 : repr_ix{repr_ix}, list_it{it} {}
    ChildrenIterator(uint8_t repr_ix, const Map_iterator& it)                  : repr_ix{repr_ix}, map_it{it} {}
    ChildrenIterator(uint8_t repr_ix, DataSource::ChunkIterator<List>* p_chit) : repr_ix{repr_ix}, ds_it{p_chit} {}
    ChildrenIterator(ChildrenIterator&& other) = default;

    ChildrenIterator& operator = (ChildrenIterator&& other) {
        repr_ix = other.repr_ix;
        switch (other.repr_ix) {
            case Object::LIST_I: list_it = std::move(other.list_it); break;
            case Object::OMAP_I: map_it = std::move(other.map_it); break;
            case Object::DSRC_I: ds_it = std::move(ds_it); break;
        }
        return *this;
    }

    ChildrenIterator& operator ++ () {
        switch (repr_ix) {
            case Object::LIST_I: ++list_it; break;
            case Object::OMAP_I: ++map_it; break;
            case Object::DSRC_I: ++ds_it; break;
            default:             throw Object::wrong_type(repr_ix);
        }
        return *this;
    }
    Object operator * () const {
        switch (repr_ix) {
            case Object::LIST_I: return *list_it;
            case Object::OMAP_I: return (*map_it).second;
            case Object::DSRC_I: return *ds_it;
            default:             throw Object::wrong_type(repr_ix);
        }
    }
    bool operator == (const ChildrenIterator& other) const {
        switch (repr_ix) {
            case Object::LIST_I: return list_it == other.list_it;
            case Object::OMAP_I: return map_it == other.map_it;
            case Object::DSRC_I: return ds_it.done();
            default:             throw Object::wrong_type(repr_ix);
        }
    }
  private:
    uint8_t repr_ix;
    List_iterator list_it;
    Map_iterator map_it;
    DataSource::ValueIterator ds_it;
};

template <class ValueType>
class Object::ChildrenRange
{
  public:
    ChildrenRange(const Object& object) : parent{object} {
        if (parent.fields.repr_ix == DSRC_I)
            p_chit = parent.repr.ds->value_iter();
    }

    ChildrenRange(ChildrenRange&& other) : parent(std::move(other.parent)), p_chit{other.p_chit} {
        other.p_it = nullptr;
    }

    auto& operator = (ChildrenRange&& other) {
        parent = std::move(other.parent);
        p_chit = other.p_chit;
        other.p_chit = nullptr;
    }

    ChildrenIterator<ValueType> begin() const {
        auto repr_ix = parent.fields.repr_ix;
        switch (repr_ix) {
            case LIST_I: return {repr_ix, std::get<0>(*parent.repr.pl).begin()};
            case OMAP_I: return {repr_ix, std::get<0>(*parent.repr.pm).begin()};
            case DSRC_I: return {repr_ix, p_chit};
            default:     return repr_ix;
        }
    }

    ChildrenIterator<ValueType> end() const {
        auto repr_ix = parent.fields.repr_ix;
        switch (repr_ix) {
            case LIST_I: return {repr_ix, std::get<0>(*parent.repr.pl).end()};
            case OMAP_I: return {repr_ix, std::get<0>(*parent.repr.pm).end()};
            case DSRC_I: return {repr_ix, nullptr};
            default:     return repr_ix;
        }
    }

  private:
    Object parent;
    DataSource::ChunkIterator<List>* p_chit = nullptr;
};


inline Object::ChildrenRange<Object> Object::iter_children() const { return *this; }
//inline Object::ChildrenRange<const Object> Object::iter_children() const { return *this; }
//inline Object::ChildrenRange<Object> Object::iter_children() { return *this; }

template <class ValueType>
class SiblingIterator
{
  public:
    using ChildrenIterator = ChildrenIterator<ValueType>;
    using ChildrenRange = Object::ChildrenRange<ValueType>;

    SiblingIterator(const ChildrenIterator& it) : it{it}, end{it} {}
    SiblingIterator(ChildrenIterator&& it) : it{std::forward<ChildrenIterator>(it)}, end{std::forward<ChildrenIterator>(it)} {}
    SiblingIterator(Object omit, const ChildrenRange& range)
      : omit{omit}, it{range.begin()}, end{range.end()} {
        if ((*it).is(omit)) ++it;
    }
    SiblingIterator& operator ++ () {
        ++it;
        if (it != end && (*it).is(omit)) ++it;
        return *this;
    }
    Object operator * () const { return *it; }
    bool operator == (const SiblingIterator& other) const { return it == other.it; }
  private:
    Object omit;
    ChildrenIterator it;
    ChildrenIterator end;
};

template <class ValueType>
class Object::SiblingRange
{
  public:
    SiblingRange(const Object& object) : omit{object}, range{object.parent()} {}
    SiblingIterator<ValueType> begin() const { return {omit, range}; }
    SiblingIterator<ValueType> end() const   { return range.end(); }
  private:
    Object omit;
    ChildrenRange<ValueType> range;
};


inline Object::SiblingRange<Object> Object::iter_siblings() const { return *this; }
//inline Object::SiblingRange<const Object> Object::iter_siblings() const { return *this; }
//inline Object::SiblingRange<Object> Object::iter_siblings() { return *this; }

template <class ValueType>
class DescendantIterator
{
  public:
    using ChildrenRange = Object::ChildrenRange<ValueType>;

    DescendantIterator() : it{(uint8_t)Object::NULL_I}, end{(uint8_t)Object::NULL_I} {}
    DescendantIterator(const ChildrenRange& range) : it{range.begin()}, end{range.end()} {
        if (it != end) push_children(*it);
    }

    DescendantIterator& operator ++ () {
        ++it;
        if (it == end) {
            if (!fifo.empty()) {
                auto& front = fifo.front();
                it = std::move(front.begin());
                end = std::move(front.end());
                fifo.pop_front();
                if (it != end) push_children(*it);
            }
        } else {
            push_children(*it);
        }
        return *this;
    }

    Object operator * () const { return *it; }
    bool operator == (const DescendantIterator& other) const { return at_end(); }

  private:
    void push_children(const Object& object) {
        auto repr_ix = object.fields.repr_ix;
        switch (repr_ix) {
            case Object::LIST_I:
            case Object::OMAP_I: fifo.emplace_back(object); break;
            default: break;
        }
    }
    bool at_end() const { return it == end && fifo.empty(); }

  private:
    ChildrenIterator<ValueType> it;
    ChildrenIterator<ValueType> end;
    std::deque<ChildrenRange> fifo;
};

template <class ValueType>
class Object::DescendantRange
{
  public:
    DescendantRange(const Object& object) : object{object} {}
    DescendantIterator<ValueType> begin() const { return object.iter_children(); }
    DescendantIterator<ValueType> end() const   { return {}; }
  private:
    Object object;
};

inline Object::DescendantRange<Object> Object::iter_descendants() const { return *this; }
//inline Object::DescendantRange<const Object> Object::iter_descendants() const { return *this; }
//inline Object::DescendantRange<Object> Object::iter_descendants() { return *this; }

//
//inline
//Path::Path(const char* spec, char delimiter) {
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
//void Path::parse(const char* spec) {
//}
//
//inline
//void Path::parse_step(const char* spec, const char*& it, char delimiter) {
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
//void Path::parse_key(const char* spec, const char*& it, char delimiter) {
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

} // nodel namespace
