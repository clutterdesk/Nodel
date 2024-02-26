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
class Result;
class Path;
class ObjectDataSource;
class KeyDataSource;

#if NODEL_ARCH == 32
using refcnt_t = uint32_t;                     // only least-significant 28-bits used
#else
using refcnt_t = uint64_t;                     // only least-significant 56-bits used
#endif

using String = std::string;
using List = std::vector<Object>;
using Map = tsl::ordered_map<Key, Object, KeyHash>;
using KeyList = std::vector<Key>;

// inplace reference count types
using IRCString = std::tuple<String, Object>;  // ref-count moved to parent bit-field
using IRCList = std::tuple<List, Object>;      // ref-count moved to parent bit-field
using IRCMap = std::tuple<Map, Object>;        // ref-count moved to parent bit-field

using StringPtr = IRCString*;
using ListPtr = IRCList*;
using MapPtr = IRCMap*;
using ObjectDataSourcePtr = ObjectDataSource*;
using KeyDataSourcePtr = KeyDataSource*;

constexpr size_t min_key_chunk_size = 128;


// NOTE: use by-value semantics
class Object
{
  public:
    class AncestorRange;
    class ChildrenRange;
    class SiblingRange;
    class DescendantRange;

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
        DSOBJ_I,  // object data-source
        DSKEY_I,  // key/value data-source
        BAD_I = 31
    };

  private:
      union Repr {
          Repr()                      : z{nullptr} {}
          Repr(bool v)                : b{v} {}
          Repr(Int v)                 : i{v} {}
          Repr(UInt v)                : u{v} {}
          Repr(Float v)               : f{v} {}
          Repr(StringPtr p)           : ps{p} {}
          Repr(ListPtr p)             : pl{p} {}
          Repr(MapPtr p)              : pm{p} {}
          Repr(ObjectDataSourcePtr p) : ods{p} {}
          Repr(KeyDataSourcePtr p)    : kds{p} {}

          void*               z;
          bool                b;
          Int                 i;
          UInt                u;
          Float               f;
          StringPtr           ps;
          ListPtr             pl;
          MapPtr              pm;
          ObjectDataSourcePtr ods;
          KeyDataSourcePtr    kds;
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
          case DSOBJ_I: return "dsobj";
          case DSKEY_I: return "dskey";
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
    Object(const Result&);

    Object(ObjectDataSourcePtr ptr)    : repr{ptr}, fields{DSOBJ_I} {}  // ownership transferred
    Object(KeyDataSourcePtr ptr)       : repr{ptr}, fields{DSKEY_I} {}  // ownership transferred

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

    ChildrenRange children() const;
    AncestorRange ancestors() const;
    SiblingRange siblings() const;
    DescendantRange descendants() const;

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
//        else if (fields.repr_ix == DSOBJ_I) return ods_read().as<T>();
//        throw wrong_type(fields.repr_ix);
//    }
//
    template <typename T>
    const T as() const requires is_byvalue<T> {
        if (fields.repr_ix == ix_conv<T>()) return val_conv<T>();
        else if (fields.repr_ix == DSOBJ_I) return ods_read().as<T>();
        throw wrong_type(fields.repr_ix);
    }

//    template <typename T>
//    T& as() requires std::is_same<T, String>::value {
//        if (fields.repr_ix == ix_conv<T>()) return val_conv<T>();
//        else if (fields.repr_ix == DSOBJ_I) return ods_read().as<T>();
//        throw wrong_type(fields.repr_ix);
//    }

    template <typename T>
    const T& as() const requires std::is_same<T, String>::value {
        if (fields.repr_ix == ix_conv<T>()) return val_conv<T>();
        else if (fields.repr_ix == DSOBJ_I) return ods_read().as<T>();
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

    const Result operator [] (is_byvalue auto) const;
    const Result operator [] (const char*) const;
    const Result operator [] (const std::string_view& v) const;
    const Result operator [] (const std::string& v) const;
    const Result operator [] (const Key& key) const;
    const Result operator [] (const Object& obj) const;
    const Result operator [] (Object&& obj) const;

    Result operator [] (is_byvalue auto);
    Result operator [] (const char*);
    Result operator [] (const std::string_view& v);
    Result operator [] (const std::string& v);
    Result operator [] (const Key& key);
    Result operator [] (const Object& obj);
    Result operator [] (Object&& obj);

    Object get(is_integral auto v) const;
    Object get(const char* v) const;
    Object get(const std::string_view& v) const;
    Object get(const std::string& v) const;
    Object get(bool v) const;
    Object get(const Key& key) const;
    Object get(const Object& obj) const;
    Object get(const Path& path) const;

    void set(const Key& key, const Object& value);
    void set(const Key& key, const Result& result);
    void set(const Object& key, const Object& value);
    void set(std::pair<const Key&, const Object&> item);
//    Object set(const Path& path, const Object& value);

    size_t size() const;

    template <typename Visitor>
    void iter_visit(Visitor&&) const;

    const KeyList keys() const;

    bool operator == (const Object&) const;
    std::partial_ordering operator <=> (const Object&) const;

    Oid id() const;
    bool is(const Object& other) const { return id() == other.id(); }
    refcnt_t ref_count() const;
    void inc_ref_count() const;
    void dec_ref_count() const;
    void release();
    void refer_to(const Object&);

    Object& operator = (const Object& other);
    Object& operator = (Object&& other);
    // TODO: Need other assignment ops to avoid Object temporaries

    bool has_data_source() const { return fields.repr_ix == DSOBJ_I || fields.repr_ix == DSKEY_I; }

    template <class DataSourceType>
    DataSourceType& data_source() const {
        switch (fields.repr_ix) {
            case DSOBJ_I: return *dynamic_cast<DataSourceType*>(repr.ods);
            case DSKEY_I: return *dynamic_cast<DataSourceType*>(repr.kds);
            default: throw wrong_type(fields.repr_ix);
        }
    }

    void reset_cache();
    void refresh_cache();

  private:
    int resolve_repr_ix() const;
    Object ods_read() const;

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
  friend class ObjectDataSource;
  friend class KeyDataSource;
  friend class WalkDF;
  friend class WalkBF;
  friend class AncestorIterator;
  friend class ChildrenIterator;
  friend class SiblingIterator;
  friend class DescendantIterator;
};

constexpr Object::ReprType null = Object::NULL_I;


// syntactic sugar
class Result
{
  public:
    using ReprType = Object::ReprType;
    using ChildrenRange = Object::ChildrenRange;
    using AncestorRange = Object::AncestorRange;
    using SiblingRange = Object::SiblingRange;
    using DescendantRange = Object::DescendantRange;

  public:
    Result(const Object& object, const Key& key) : object{object}, latent_key{key} {}
    Result(const Object& object, Key&& key)      : object{object}, latent_key{std::forward<Key>(key)} {}

    explicit Result(const Result& other) : object{other.object}, latent_key{other.latent_key} {}
    explicit Result(Result&& other)      : object{other.object}, latent_key{std::forward<Key>(other.latent_key)} {}

    ReprType type() const         { return resolve().type(); }
    Object root() const           { return object.root(); }
    Object parent() const         { return object; }
    ChildrenRange children() const;
    AncestorRange ancestors() const;
    SiblingRange siblings() const;
    DescendantRange descendants() const;

    const Key key() const { return resolve().key(); }
    Path path() const;

    template <typename T> T value_cast() const          { return resolve().value_cast<T>(); }
    template <typename T> bool is_type() const          { return resolve().is_type<T>(); }
    template <typename V> void visit(V&& visitor) const { return resolve().visit(visitor); }

    template <typename T> const T as() const  requires is_byvalue<T>                  { return resolve().as<T>(); }
    template <typename T> T as()              requires is_byvalue<T>                  { return resolve().as<T>(); }
    template <typename T> const T& as() const requires std::is_same<T, String>::value { return resolve().as<T>(); }

    bool is_empty() const     { return resolve().is_empty(); }
    bool is_null() const      { return resolve().is_null(); }
    bool is_bool() const      { return resolve().is_bool(); }
    bool is_int() const       { return resolve().is_int(); }
    bool is_uint() const      { return resolve().is_uint(); }
    bool is_float() const     { return resolve().is_float(); }
    bool is_str() const       { return resolve().is_str(); }
    bool is_num() const       { return resolve().is_num(); }
    bool is_list() const      { return resolve().is_list(); }
    bool is_map() const       { return resolve().is_map(); }
    bool is_container() const { return resolve().is_container(); }

    bool to_bool() const        { return resolve().to_bool(); }
    Int to_int() const          { return resolve().to_int(); }
    UInt to_uint() const        { return resolve().to_uint(); }
    Float to_float() const      { return resolve().to_float(); }
    std::string to_str() const  { return resolve().to_str(); }
    Key to_key() const          { return resolve().to_key(); }
    Key to_tmp_key() const      { return resolve().to_tmp_key(); }
    Key into_key() const        { return resolve().into_key(); }
    std::string to_json() const { return resolve().to_json(); }

    const Result operator [] (is_byvalue auto v) const         { return {resolve(), Key{v}}; }
    const Result operator [] (const char* v) const             { return {resolve(), Key{v}}; }
    const Result operator [] (const std::string_view& v) const { return {resolve(), Key{v}}; }
    const Result operator [] (const std::string& v) const      { return {resolve(), Key{v}}; }
    const Result operator [] (const Key& key) const            { return {resolve(), key}; }
    const Result operator [] (const Object& obj) const         { return {resolve(), obj.to_key()}; }
    const Result operator [] (Object&& obj) const              { return {resolve(), obj.into_key()}; }

    Result operator [] (is_byvalue auto v)          { return {resolve(), Key{v}}; }
    Result operator [] (const char* v)              { return {resolve(), Key{v}}; }
    Result operator [] (const std::string_view& v)  { return {resolve(), Key{v}}; }
    Result operator [] (const std::string& v)       { return {resolve(), Key{v}}; }
    Result operator [] (const Key& key)             { return {resolve(), key}; }
    Result operator [] (const Object& obj)          { return {resolve(), obj.to_key()}; }
    Result operator [] (Object&& obj)               { return {resolve(), obj.into_key()}; }

    Object get(is_integral auto v) const        { return resolve().get(v); }
    Object get(const char* v) const             { return resolve().get(v); }
    Object get(const std::string_view& v) const { return resolve().get(v); }
    Object get(const std::string& v) const      { return resolve().get(v); }
    Object get(bool v) const                    { return resolve().get(v); }
    Object get(const Key& key) const            { return resolve().get(key); }
    Object get(const Object& obj) const         { return resolve().get(obj); }
    Object get(const Path& path) const          { return resolve().get(path); }

    void set(const Key& key, const Object& value)       { resolve().set(key, value); }
    void set(const Object& key, const Object& value)    { resolve().set(key, value); }
    void set(std::pair<const Key&, const Object&> item) { resolve().set(item); }
//    Object set(const Path& path, const Object& value)   { resolve().set(path, value); }

    size_t size() const { return resolve().size(); }

    template <typename Visitor>
    void iter_visit(Visitor&& visitor) const { resolve().iter_visit(visitor); }

    const KeyList keys() const { return resolve().keys(); }

    bool operator == (const Object& obj) const                   { return resolve() == obj; }
    std::partial_ordering operator <=> (const Object& obj) const { return resolve() <=> obj; }

    Oid id() const                         { return resolve().id(); }
    bool is(const Object& other) const     { return resolve().is(other); }

    void release()                   { latent_key = Key{}; object.release(); }
    void refer_to(const Object& obj) { latent_key = Key{}; object.refer_to(obj); }

    Object operator = (const Object& other) {
        if (latent_key.is_null())
            object = other;
        else
            object.set(latent_key, other);
        return object;
    }

    Object operator = (Object&& other) {
        if (latent_key.is_null())
            object = std::forward<Object>(other);
        else
            object.set(latent_key, std::forward<Object>(other));
        return object;
    }

    Object operator = (const Result& other) {
        if (latent_key.is_null())
            object = other.resolve();
        else
            object.set(latent_key, other);
        return object;
    }
    // TODO: Need other assignment ops to avoid Object temporaries

    bool has_data_source() const { return resolve().has_data_source(); }

    template <class DataSourceType>
    DataSourceType& data_source() const { return resolve().data_source<DataSourceType>(); }

    void reset_cache()   { resolve().reset_cache(); }
    void refresh_cache() { resolve().refresh_cache(); }

    operator Object () const { return resolve(); }
    operator Object () { return resolve(); }

    bool is_resolved() const { return latent_key.is_null(); }

    Object resolve() const {
        if (is_resolved()) return object;
        Result& self = *const_cast<Result*>(this);
        self.object.refer_to(object.get(latent_key));
        self.latent_key = Key{};
        return self.object;
    }

  private:
    Object object;
    Key latent_key;
};


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

    std::string to_str() const {
        std::stringstream ss;
        for (auto& key : keys)
            key.to_step(ss);
        return ss.str();
    }

  private:
    KeyList keys;
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


inline
Path Result::path() const {
    if (is_resolved()) return object.path();
    Path path{object.path()};
    path.append(latent_key);
    return path;
}


class ChangeSet
{
  public:
    using Record = std::tuple<Key, Object, Path>;

  public:
    ChangeSet() = default;

    void add(const Key& key, const Object& object, const Path& parent_path) {
        records.emplace_back(key, object, parent_path);
    }

    void clear() {
        records.clear();
    }

    void apply(Object& object) {
        for (auto& [key, value, parent_path] : records) {
            // parent_path is irrelevant, here
            object.set(key, value);
        }
    }

  private:
    std::vector<Record> records;
};


// TODO: move into DataSource class namespace
class IDataSourceIterator
{
  public:
    virtual ~IDataSourceIterator() {}
    virtual std::string* iter_begin_str()  { return nullptr; }
    virtual List* iter_begin_list()        { return nullptr; }
    virtual KeyList* iter_begin_key_list() { return nullptr; }
    virtual size_t iter_next() = 0;
    virtual void iter_end() = 0;

  public:
    struct Context
    {
        Context() : p_iter{nullptr} {}
        Context(IDataSourceIterator* p_iter) : p_iter{p_iter} {}
        Context(Context&& other) : p_iter{other.p_iter} { other.p_iter = nullptr; }
        ~Context() { if (p_iter != nullptr) { p_iter->iter_end(); delete p_iter; } }
        IDataSourceIterator* p_iter;
    };
};

class DataSource
{
  public:
    virtual ~DataSource() {}

    virtual void read_meta(Object&) = 0;    // load meta-data including type and id

  private:
    Object get_parent() const           { return parent; }
    void set_parent(Object& new_parent) { parent.refer_to(new_parent); }

    refcnt_t ref_count() const { return parent.fields.ref_count; }
    void inc_ref_count()       { parent.fields.ref_count++; }
    refcnt_t dec_ref_count()   { return --(parent.fields.ref_count); }

  private:
    Object parent;  // ref-count moved to parent bit-field

  protected:
    Object cache;  // implementations must cache data here

  friend class Object;
};

class ObjectDataSource : public DataSource
{
  public:
    using ReprType = Object::ReprType;

    virtual void read(Object& cache) = 0;
    virtual void write(const Object&) = 0;
    virtual void write(Object&&) = 0;

    virtual IDataSourceIterator* new_iter() const { return nullptr; }  // default implementation has no iterator

    const Object& get_cached() const { return const_cast<ObjectDataSource&>(*this).get_cached(); }
    Object& get_cached()             { insure_cached(); return cache; }

    size_t size()              { insure_cached(); return cache.size(); }
    Object get(const Key& key) { insure_cached(); return cache.get(key); }

    void set(const Key& key, const Object& value) {
        if (!is_cached) {
            records.add(key, value, value.parent().path());
        } else {
            cache.set(key, value);
        }
    }

    const Key key_of(const Object& obj) { assert (is_cached); return cache.key_of(obj); }

    ReprType type() { if (cache.is_empty()) read_meta(cache); return cache.type(); }
    Oid id()        { if (cache.is_empty()) read_meta(cache); return cache.id(); }

    void reset()   { cache.release(); is_cached = false; }
    void refresh() {}  // TODO: implement

  private:
    void insure_cached() {
        if (!is_cached) {
            read(cache);
            records.apply(cache);
            records.clear();
            is_cached = true;
        }
    }

  private:
    ChangeSet records;
    bool is_cached;

  friend class Object;
};

class KeyDataSource : public DataSource
{
  public:
    using ReprType = Object::ReprType;

    virtual Object read_key(const Key&) = 0;
    virtual void write_key(const Key&, const Object&) = 0;
    virtual void write_key(const Key&, Object&&) = 0;

    virtual KeyList keys() = 0;
    virtual size_t size() = 0;

    virtual IDataSourceIterator* new_iter() const = 0;

    const Object& get_partial_cached() const { return cache; }
    Object& get_partial_cached()             { return cache; }

    Object get(const Key& key) {
        // KeyDataSources always know their type upfront before accessing external storage,
        // so just call read_meta to initialize the cache object.
        if (cache.is_empty()) read_meta(cache);

        // Cache object is an incomplete map containing only the keys that have been loaded.
        // In this context, a key mapped to a null value indicates a key that has been loaded,
        // but is not defined in external storage.  A refresh will update keys that materialized
        // after first being accessed and before the call to refresh().
        Object value = cache.get(key);
        if (value.is_empty()) {
            value = read_key(key);
            cache.set(key, value);
        }

        return value;
    }

    void set(const Key& key, const Object& value) {
        if (cache.is_empty()) read_meta(cache);
        cache.set(key, value);
    }

    const Key key_of(const Object& obj) { return cache.key_of(obj); }

    ReprType type() { if (cache.is_empty()) read_meta(cache); return cache.type(); }
    Oid id()        { if (cache.is_empty()) read_meta(cache); return cache.id(); }

    void reset()   { cache.release(); }
    void refresh() {}  // TODO: implement

  friend class Object;
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
        case DSOBJ_I: repr.ods = other.repr.ods; repr.ods->inc_ref_count(); break;
        case DSKEY_I: repr.kds = other.repr.kds; repr.kds->inc_ref_count(); break;
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
        case DSOBJ_I: repr.ods = other.repr.ods; break;
        case DSKEY_I: repr.kds = other.repr.kds; break;
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
        case DSOBJ_I: return repr.ods->type();
        case DSKEY_I: return repr.kds->type();
        default:      return (ReprType)fields.repr_ix;
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
        case DSOBJ_I: return repr.ods->get_parent();
        case DSKEY_I: return repr.kds->get_parent();
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
        case DSOBJ_I: repr.ods->get_cached().visit(std::forward<V>(visitor)); break;
        case DSKEY_I: // TODO: visit a wrapper API (also LIST_I, MAP_I, and other containers)
        default:      throw wrong_type(fields.repr_ix);
    }
}

inline
int Object::resolve_repr_ix() const {
    switch (fields.repr_ix) {
        case DSOBJ_I: return const_cast<ObjectDataSourcePtr>(repr.ods)->type();
        case DSKEY_I: return const_cast<KeyDataSourcePtr>(repr.kds)->type();
        default:      return fields.repr_ix;
    }
}

inline
Object Object::ods_read() const {
    return const_cast<ObjectDataSourcePtr>(repr.ods)->get_cached();
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
            parent.release();
            parent = new_parent;
            break;
        }
        case LIST_I: {
            auto& parent = std::get<1>(*repr.pl);
            parent.release();
            parent = new_parent;
            break;
        }
        case OMAP_I: {
            auto& parent = std::get<1>(*repr.pm);
            parent.release();
            parent = new_parent;
            break;
        }
        case DSOBJ_I: {
            auto parent = repr.ods->get_parent();
            parent.release();
            parent = new_parent;
            break;
        }
        case DSKEY_I: {
            auto parent = repr.kds->get_parent();
            parent.release();
            parent = new_parent;
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
        case DSOBJ_I: {
            return repr.ods->key_of(obj);
        }
        case DSKEY_I: {
            return repr.kds->key_of(obj);
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
        case DSOBJ_I: return repr.ods->get_cached().value_cast<T>();
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
        case DSOBJ_I: return const_cast<ObjectDataSourcePtr>(repr.ods)->get_cached().to_str();
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
        case DSOBJ_I: return const_cast<ObjectDataSourcePtr>(repr.ods)->get_cached().to_bool();
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
        case DSOBJ_I: return const_cast<ObjectDataSourcePtr>(repr.ods)->get_cached().to_int();
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
        case DSOBJ_I: return const_cast<ObjectDataSourcePtr>(repr.ods)->get_cached().to_uint();
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
        case DSOBJ_I: return const_cast<ObjectDataSourcePtr>(repr.ods)->get_cached().to_float();
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
        case DSOBJ_I: return const_cast<ObjectDataSourcePtr>(repr.ods)->get_cached().to_key();
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
        case DSOBJ_I: return const_cast<ObjectDataSourcePtr>(repr.ods)->get_cached().to_tmp_key();
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
        case DSOBJ_I: {
            Key k{repr.ods->get_cached().into_key()};
            release();
            return k;
        }
        default:
            throw wrong_type(fields.repr_ix);
    }
}

inline const Result Object::operator [] (is_byvalue auto v) const         { return {*this, Key{v}}; }
inline const Result Object::operator [] (const char* v) const             { return {*this, Key{v}}; }
inline const Result Object::operator [] (const std::string_view& v) const { return {*this, Key{v}}; }
inline const Result Object::operator [] (const std::string& v) const      { return {*this, Key{v}}; }
inline const Result Object::operator [] (const Key& key) const            { return {*this, key}; }
inline const Result Object::operator [] (const Object& obj) const         { return {*this, obj.to_key()}; }
inline const Result Object::operator [] (Object&& obj) const              { return {*this, obj.into_key()}; }

inline Result Object::operator [] (is_byvalue auto v)         { return {*this, Key{v}}; }
inline Result Object::operator [] (const char* v)             { return {*this, Key{v}}; }
inline Result Object::operator [] (const std::string_view& v) { return {*this, Key{v}}; }
inline Result Object::operator [] (const std::string& v)      { return {*this, Key{v}}; }
inline Result Object::operator [] (const Key& key)            { return {*this, key}; }
inline Result Object::operator [] (const Object& obj)         { return {*this, obj.to_key()}; }
inline Result Object::operator [] (Object&& obj)              { return {*this, obj.into_key()}; }

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
        case DSOBJ_I: return repr.ods->get(v);
        case DSKEY_I: return repr.kds->get(v);
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
        case DSOBJ_I: return repr.ods->get(v);
        case DSKEY_I: return repr.kds->get(v);
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
        case DSOBJ_I: return repr.ods->get(v);
        case DSKEY_I: return repr.kds->get(v);
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
        case DSOBJ_I: return repr.ods->get(index);
        case DSKEY_I: return repr.kds->get(index);
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
        case DSOBJ_I: return repr.ods->get(v);
        case DSKEY_I: return repr.kds->get(v);
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
        case DSOBJ_I: return repr.ods->get(key);
        case DSKEY_I: return repr.kds->get(key);
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
        case DSOBJ_I: return repr.ods->get(obj.to_tmp_key());
        case DSKEY_I: return repr.kds->get(obj.to_tmp_key());
        default:      throw wrong_type(fields.repr_ix);
    }
}

inline
Object Object::get(const Path& path) const {
    return path.lookup(*this);
}

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
        case DSOBJ_I: return repr.ods->set(key, value);
        case DSKEY_I: return repr.kds->set(key, value);
        default:
            throw wrong_type(fields.repr_ix);
    }
}

inline
void Object::set(const Key& key, const Result& result) {
    switch (fields.repr_ix) {
        case EMPTY_I: throw empty_reference(__FUNCTION__);
        case LIST_I:
        case OMAP_I:  set(key, result.resolve()); break;
        case DSOBJ_I: return repr.ods->set(key, result.resolve());
        case DSKEY_I: return repr.kds->set(key, result.resolve());
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
    // create intermediates
//}

inline
size_t Object::size() const {
    switch (fields.repr_ix) {
        case EMPTY_I: throw empty_reference(__FUNCTION__);
        case STR_I:   return std::get<0>(*repr.ps).size();
        case LIST_I:  return std::get<0>(*repr.pl).size();
        case OMAP_I:  return std::get<0>(*repr.pm).size();
        case DSOBJ_I:  return repr.ods->size();
        case DSKEY_I:  return repr.kds->size();
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
        case DSOBJ_I: {
            auto ods = repr.ods;
            auto p_iter = ods->new_iter();
            if (p_iter == nullptr)
            {
                // cache, then iterate - intended for data-sources without iteration capability
                ods->get_cached().iter_visit(visit);
            }
            else
            {
                IDataSourceIterator::Context context{p_iter};
                auto type = ods->type();
                switch (type) {
                    case STR_I: {
                        auto& chunk = *(p_iter->iter_begin_str());
                        while (p_iter->iter_next() > 0) {
                            for (const auto c : chunk)
                                if (!visit(c))
                                    return;
                        }
                        break;
                    }
                    case LIST_I: {
                        auto& chunk = *(p_iter->iter_begin_list());
                        while (p_iter->iter_next() > 0) {
                            for (auto& obj : chunk) {
                                if (!visit(obj))
                                    return;
                            }
                        }
                        break;
                    }
                    case OMAP_I: {
                        auto& chunk = *(p_iter->iter_begin_key_list());
                        while (p_iter->iter_next() > 0) {
                            for (auto& key : chunk) {
                                if (!visit(key))
                                    return;
                            }
                        }
                        break;
                    }
                    default:
                        throw wrong_type(type);
                }
            }
            return;
        }
        case DSKEY_I: {
            // TODO: iterator KV pairs, instead?
            auto kds = repr.kds;
            auto p_iter = kds->new_iter();
            assert (p_iter != nullptr);
            IDataSourceIterator::Context context{p_iter};
            auto& chunk = *(p_iter->iter_begin_key_list());
            while (p_iter->iter_next() > 0) {
                for (auto& key : chunk) {
                    if (!visit(key))
                        return;
                }
            }
            return;
        }
        default:
            throw wrong_type(fields.repr_ix);
    }
}

inline
const KeyList Object::keys() const {
    switch (fields.repr_ix) {
        case OMAP_I:  {
            KeyList keys;
            for (auto& item : std::get<0>(*repr.pm)) {
                keys.push_back(std::get<0>(item));
            }
            return keys;
        }
        case DSOBJ_I: return repr.ods->get_cached().keys();
        case DSKEY_I: return repr.kds->keys();
        default:      throw wrong_type(fields.repr_ix);
    }
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
        case DSOBJ_I: return const_cast<ObjectDataSourcePtr>(repr.ods)->get_cached() == obj;
        default:      throw wrong_type(fields.repr_ix);
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
        case DSOBJ_I: return const_cast<ObjectDataSourcePtr>(repr.ods)->get_cached() <=> obj;
        default:      throw wrong_type(fields.repr_ix);
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
                    // TODO: revisit how DSOBJ_I and DSKEY_I are handled - parameters to determine caching behavior
                    case Object::DSOBJ_I: {
                        stack.emplace(parent, key, object.repr.ods->get_cached(), FIRST_VALUE);
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
                case Object::DSOBJ_I: {
                    deque.emplace_front(parent, key, object.repr.ods->get_cached());
                    break;
                }
                // TODO: revisit how DSOBJ_I and DSKEY_I are handled - parameters to determine caching behavior
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
        case DSOBJ_I: return repr.ods->get_cached().id();
        case DSKEY_I: return repr.kds->get_partial_cached().id();
        default:      throw wrong_type(fields.repr_ix);
    }
}

inline
refcnt_t Object::ref_count() const {
    switch (fields.repr_ix) {
        case STR_I:   return std::get<1>(*repr.ps).fields.ref_count;
        case LIST_I:  return std::get<1>(*repr.pl).fields.ref_count;
        case OMAP_I:  return std::get<1>(*repr.pm).fields.ref_count;
        case DSOBJ_I: return repr.ods->ref_count();
        case DSKEY_I: return repr.kds->ref_count();
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
        case DSOBJ_I: repr.ods->inc_ref_count(); break;
        case DSKEY_I: repr.kds->inc_ref_count(); break;
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
        case DSOBJ_I: if (repr.ods->dec_ref_count() == 0) delete self.repr.ods; break;
        case DSKEY_I: if (repr.kds->dec_ref_count() == 0) delete self.repr.kds; break;
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
            case DSOBJ_I: {
                repr.ods = other.repr.ods;
                inc_ref_count();
                break;
            }
            case DSKEY_I: {
                repr.kds = other.repr.kds;
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
                case DSOBJ_I: {
                    repr.ods->write(other);
                    break;
                }
                case DSKEY_I: throw wrong_type(fields.repr_ix);
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
            case DSOBJ_I: repr.ods->write(other); break;
            case DSKEY_I: throw wrong_type(repr_ix);
            default:     break;
        }

        if (repr_ix != DSOBJ_I) {
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
            case DSOBJ_I: repr.ods = other.repr.ods; break;
            case DSKEY_I: repr.kds = other.repr.kds; break;
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
                case DSOBJ_I: repr.ods->write(other); break;
                case DSKEY_I: throw wrong_type(fields.repr_ix);
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
            case DSOBJ_I: repr.ods->write(other); break;
            case DSKEY_I: throw wrong_type(repr_ix);
            default:     break;
        }

        if (repr_ix != DSOBJ_I) {
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
void Object::reset_cache() {
    switch (fields.repr_ix) {
        case DSOBJ_I: repr.ods->reset(); break;
        case DSKEY_I: repr.kds->reset(); break;
        default:      break;
    }
}

inline
void Object::refresh_cache() {
    switch (fields.repr_ix) {
        case DSOBJ_I: repr.ods->refresh(); break;
        case DSKEY_I: repr.kds->refresh(); break;
        default:      break;
    }
}


class AncestorIterator
{
  public:
    AncestorIterator(const Object& object) : object{object} {}
    AncestorIterator(Object&& object) : object{std::forward<Object>(object)} {}

    AncestorIterator(const AncestorIterator&) = default;
    AncestorIterator(AncestorIterator&&) = default;

    AncestorIterator& operator ++ () {
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

class Object::AncestorRange
{
  public:
    AncestorRange(const Object& object) : object{object.parent()} {}
    AncestorIterator begin() const { return object; }
    AncestorIterator end() const   { return Object{}; }
  private:
    Object object;
};

inline
Object::AncestorRange Object::ancestors() const {
    return *this;
}

inline
Object::AncestorRange Result::ancestors() const {
    return resolve();  // TODO: return object.ancestors_or_self()
}

class ChildrenIterator
{
  public:
    ChildrenIterator(uint8_t repr_ix) : repr_ix{repr_ix} {}
    ChildrenIterator(uint8_t repr_ix, const List::iterator& iter) : repr_ix{repr_ix}, list_iter{iter} {}
    ChildrenIterator(uint8_t repr_ix, const Map::iterator& iter) : repr_ix{repr_ix}, map_iter{iter} {}
    ChildrenIterator(uint8_t repr_ix, IDataSourceIterator* p_iter) : repr_ix{repr_ix}, p_ds_iter{p_iter} {
        p_chunk = p_ds_iter->iter_begin_list();
        list_iter = p_chunk->begin();
        chunk_end = p_chunk->end();
    }
    ChildrenIterator(const ChildrenIterator& other)
      : repr_ix{other.repr_ix},
        list_iter{other.list_iter},
        map_iter{other.map_iter},
        p_ds_iter{nullptr}
    {}
    ChildrenIterator(ChildrenIterator&& other)
      : repr_ix{other.repr_ix},
        list_iter{std::move(other.list_iter)},
        map_iter{std::move(other.map_iter)},
        p_ds_iter{other.p_ds_iter} {
        other.p_ds_iter = nullptr;
    }
    ChildrenIterator& operator = (ChildrenIterator&& other) {
        repr_ix = other.repr_ix;
        switch (other.repr_ix) {
            case Object::LIST_I: list_iter = std::move(other.list_iter); break;
            case Object::OMAP_I: map_iter = std::move(other.map_iter); break;
            case Object::DSOBJ_I: p_ds_iter = other.p_ds_iter; other.p_ds_iter = nullptr; break;
            case Object::DSKEY_I: p_ds_iter = other.p_ds_iter; other.p_ds_iter = nullptr; break;
        }
        return *this;
    }
    ChildrenIterator& operator ++ () {
        switch (repr_ix) {
            case Object::LIST_I: ++list_iter; break;
            case Object::OMAP_I: ++map_iter; break;
            case Object::DSOBJ_I:
            case Object::DSKEY_I: {
                if (list_iter == chunk_end) {
                    if (p_ds_iter->iter_next() == 0)
                        return *this;
                    list_iter = p_chunk->begin();
                    chunk_end = p_chunk->end();
                }
                ++list_iter;
                break;
            }
            default: assert (false);
        }
        return *this;
    }
    Object operator * () const {
        switch (repr_ix) {
            case Object::LIST_I:  return *list_iter;
            case Object::OMAP_I:  return std::get<1>(*map_iter);
            case Object::DSOBJ_I:
            case Object::DSKEY_I: return *list_iter;
            default: assert (false);
        }
    }
    bool operator == (const ChildrenIterator& other) const {
        switch (repr_ix) {
            case Object::LIST_I:  return list_iter == other.list_iter;
            case Object::OMAP_I:  return map_iter == other.map_iter;
            case Object::DSOBJ_I:
            case Object::DSKEY_I: return list_iter == chunk_end;
            default: assert (other.repr_ix == repr_ix); return true;
        }
    }
  private:
    uint8_t repr_ix;
    List::iterator list_iter;
    Map::iterator map_iter;
    IDataSourceIterator* p_ds_iter;
    List* p_chunk = nullptr;
    List::iterator chunk_end;
};

class Object::ChildrenRange
{
  public:
    ChildrenRange(const Object& object) : parent{object} {
        switch (parent.fields.repr_ix) {
            case DSOBJ_I: p_ds_iter = parent.repr.ods->new_iter(); break;
            case DSKEY_I: p_ds_iter = parent.repr.kds->new_iter(); break;
            default:      break;
        }
    }

    ChildrenRange(ChildrenRange&& other) : parent(std::move(other.parent)), p_ds_iter{other.p_ds_iter} {
        other.p_ds_iter = nullptr;
    }

    ~ChildrenRange() {
        if (p_ds_iter != nullptr) {
            p_ds_iter->iter_end();
            delete p_ds_iter;
        }
    }

    ChildrenRange(const ChildrenRange&) = delete;
    auto& operator = (const ChildrenRange&) = delete;
    auto& operator = (ChildrenRange&&) = delete;

    ChildrenIterator begin() const {
        auto repr_ix = parent.fields.repr_ix;
        switch (repr_ix) {
            case LIST_I:  return {repr_ix, std::get<0>(*parent.repr.pl).begin()};
            case OMAP_I:  return {repr_ix, std::get<0>(*parent.repr.pm).begin()};
            case DSOBJ_I:
            case DSKEY_I: return {repr_ix, p_ds_iter};
            default:      return repr_ix;
        }
    }

    ChildrenIterator end() const {
        auto repr_ix = parent.fields.repr_ix;
        switch (repr_ix) {
            case LIST_I:  return {repr_ix, std::get<0>(*parent.repr.pl).end()};
            case OMAP_I:  return {repr_ix, std::get<0>(*parent.repr.pm).end()};
            case DSOBJ_I:
            case DSKEY_I: return {repr_ix, nullptr};
            default:      return repr_ix;
        }
    }

  private:
    Object parent;
    IDataSourceIterator* p_ds_iter = nullptr;
};

inline
Object::ChildrenRange Object::children() const {
    return *this;
}

inline
Object::ChildrenRange Result::children() const {
    return resolve();
}

class SiblingIterator
{
  public:
    SiblingIterator(const ChildrenIterator& iter) : iter{iter}, end{iter} {}
    SiblingIterator(Oid skip_oid, const Object::ChildrenRange& range)
      : skip_oid{skip_oid}, iter{range.begin()}, end{range.end()} {
        if ((*iter).id() == skip_oid) ++iter;
    }
    SiblingIterator& operator ++ () {
        ++iter;
        if (iter != end && (*iter).id() == skip_oid) ++iter;
        return *this;
    }
    Object operator * () const { return *iter; }
    bool operator == (const SiblingIterator& other) const { return iter == other.iter; }
  private:
    Oid skip_oid;
    ChildrenIterator iter;
    ChildrenIterator end;
};

class Object::SiblingRange
{
  public:
    SiblingRange(const Object& object) : skip_oid{object.id()}, range{object.parent()} {}
    SiblingIterator begin() const { return {skip_oid, range}; }
    SiblingIterator end() const   { return range.end(); }
  private:
    Oid skip_oid;
    ChildrenRange range;
};

inline
Object::SiblingRange Object::siblings() const {
    return *this;
}

inline
Object::SiblingRange Result::siblings() const {
    return resolve();
}

class DescendantIterator
{
  public:
    DescendantIterator() : iter{(uint8_t)Object::NULL_I}, end{(uint8_t)Object::NULL_I} {}
    DescendantIterator(const Object::ChildrenRange& range) : iter{range.begin()}, end{range.end()} {
        if (iter != end) push_children(*iter);
    }
    DescendantIterator& operator ++ () {
        ++iter;
        if (iter == end) {
            if (!fifo.empty()) {
                auto& front = fifo.front();
                iter = front.begin();
                end = front.end();
                fifo.pop_front();
                if (iter != end) push_children(*iter);
            }
        } else {
            push_children(*iter);
        }
        return *this;
    }
    Object operator * () const { return *iter; }
    bool operator == (const DescendantIterator& other) const {
        if (at_end()) return other.at_end();
        return false;
    }
  private:
    void push_children(const Object& object) {
        auto repr_ix = object.fields.repr_ix;
        switch (repr_ix) {
            case Object::LIST_I:
            case Object::OMAP_I: fifo.emplace_back(object); break;
            default: break;
        }
    }
    bool at_end() const { return iter == end && fifo.empty(); }
  private:
    ChildrenIterator iter;
    ChildrenIterator end;
    std::deque<Object::ChildrenRange> fifo;
};

class Object::DescendantRange
{
  public:
    DescendantRange(const Object& object) : object{object} {}
    DescendantIterator begin() const { return object.children(); }
    DescendantIterator end() const   { return {}; }
  private:
    Object object;
};

inline
Object::DescendantRange Object::descendants() const {
    return *this;
}

inline
Object::DescendantRange Result::descendants() const {
    return resolve();
}

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
