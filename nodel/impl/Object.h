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
#include <fmt/core.h>

#include "support.h"
#include "str_util.h"
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

struct Json
{
    Json(std::string_view&& text) : text{std::move(text)} {}
    Json(const Json&) = delete;
    Json(Json&&) = delete;
    std::string_view text;
};

class Object;
class ISource;
class Path;

#if NODEL_ARCH == 32
using refcnt_t = uint32_t;                     // only least-significant 28-bits used
#else
using refcnt_t = uint64_t;                     // only least-significant 56-bits used
#endif

using String = std::string;
using List = std::vector<Object>;
using Map = tsl::ordered_map<Key, Object, KeyHash>;

// inplace reference count types
using IRCString = std::tuple<String, Object>;  // ref-count moved to parent bit-field (brain twister, but correct)
using IRCList = std::tuple<List, Object>;      // ref-count moved to parent bit-field (brain twister, but correct)
using IRCMap = std::tuple<Map, Object>;        // ref-count moved to parent bit-field (brain twister, but correct)

using StringPtr = IRCString*;
using ListPtr = IRCList*;
using MapPtr = IRCMap*;
using ISourcePtr = ISource*;

constexpr const char* null = nullptr;


// NOTE: use by-value semantics
class Object
{
  public:
      enum ReprType {
          EMPTY_I,  // uninitialized
          NULL_I,   // json null
          BOOL_I,
          INT_I,
          UINT_I,
          FLOAT_I,
          STR_I,
          SSTR_I,
          LIST_I,
          MAP_I,
          DSRC_I
      };

  private:
      union Repr {
          Repr()            : z{nullptr} {}
          Repr(bool v)      : b{v} {}
          Repr(Int v)       : i{v} {}
          Repr(UInt v)      : u{v} {}
          Repr(Float v)     : f{v} {}
          Repr(StringPtr p) : ps{p} {}
          Repr(ListPtr p)   : pl{p} {}
          Repr(MapPtr p)    : pm{p} {}

          void*      z;
          bool       b;
          Int        i;
          UInt       u;
          Float      f;
          StringPtr  ps;
          ListPtr    pl;
          MapPtr     pm;
          ISourcePtr ds;
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
          uint8_t repr_ix:4;
          uint32_t ref_count:28;
#else
          uint8_t repr_ix:8;
          uint64_t ref_count:56;
#endif
      };

  private:
    struct NoParent {};
    Object(const NoParent&)     : repr{}, fields{NULL_I, 1} {}  // initialize reference count
    Object(NoParent&&)          : repr{}, fields{NULL_I, 1} {}  // initialize reference count

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
          case MAP_I:   return "map";
          case DSRC_I:  return "dsrc";
          default:      throw std::logic_error("invalid repr_ix");
      }
    }

  public:
    static constexpr refcnt_t no_ref_count = std::numeric_limits<refcnt_t>::max();

    Object(const char* v) : repr{}, fields{EMPTY_I} {
        if (v == nullptr) {
            fields.repr_ix = NULL_I;
            repr.z = nullptr;
        } else {
            fields.repr_ix = STR_I;
            repr.ps = new IRCString{v, {}};
        }
    }

    Object()                           : repr{}, fields{EMPTY_I} {}
    Object(const std::string& str)     : repr{new IRCString{str, {}}}, fields{STR_I} {}
    Object(std::string&& str)          : repr{new IRCString(std::move(str), {})}, fields{STR_I} {}
    Object(const std::string_view& sv) : repr{new IRCString{{sv.data(), sv.size()}, {}}}, fields{STR_I} {}
    Object(bool v)                     : repr{v}, fields{BOOL_I} {}
    Object(is_like_Float auto v)       : repr{(Float)v}, fields{FLOAT_I} {}
    Object(is_like_Int auto v)         : repr{(Int)v}, fields{INT_I} {}
    Object(is_like_UInt auto v)        : repr{(UInt)v}, fields{UINT_I} {}
    Object(List&& list);
    Object(Map&& map);
    Object(const Key& key);
    Object(Key&& key);
    Object(Json&& json);  // implemented in nodel.h

    Object(const Object& other);
    Object(Object&& other);

    ~Object() { free(); }

    const Object parent() const;
    Object parent();

    template <typename T>
    T value_cast() const;

    template <typename T>
    bool is_type() const;
    template <> bool is_type<bool>() const   { return repr_ix() == BOOL_I; }
    template <> bool is_type<Int>() const    { return repr_ix() == INT_I; }
    template <> bool is_type<UInt>() const   { return repr_ix() == UINT_I; }
    template <> bool is_type<Float>() const  { return repr_ix() == FLOAT_I; }
    template <> bool is_type<String>() const { return repr_ix() == STR_I; }
    template <> bool is_type<List>() const   { return repr_ix() == LIST_I; }
    template <> bool is_type<Map>() const    { return repr_ix() == MAP_I; }

    template <typename V>
    void visit(V&& visitor) const;

    template <typename T>
    T& as();
    template <> bool& as<bool>() {
        if (fields.repr_ix != BOOL_I) {
            if (fields.repr_ix == DSRC_I)
                return dsrc_read().as<bool>();
            throw wrong_type(fields.repr_ix, BOOL_I);
        }
        return repr.b;
    }

    template <> Int& as<Int>() {
        if (fields.repr_ix != INT_I) {
            if (fields.repr_ix == DSRC_I)
                return dsrc_read().as<Int>();
            throw wrong_type(fields.repr_ix, INT_I);
        }
        return repr.i;
    }

    template <> UInt& as<UInt>() {
        if (fields.repr_ix != UINT_I) {
            if (fields.repr_ix == DSRC_I)
                return dsrc_read().as<UInt>();
            throw wrong_type(fields.repr_ix, UINT_I);
        }
        return repr.u;
    }

    template <> Float& as<Float>() {
        if (fields.repr_ix != FLOAT_I) {
            if (fields.repr_ix == DSRC_I)
                return dsrc_read().as<Float>();
            throw wrong_type(fields.repr_ix, FLOAT_I);
        }
        return repr.f;
    }

    template <> String& as<String>() {
        if (fields.repr_ix != STR_I) {
            if (fields.repr_ix == DSRC_I)
                return dsrc_read().as<String>();
            throw wrong_type(fields.repr_ix, STR_I);
        }
        return std::get<0>(*repr.ps);
    }

    template <typename T>
    const T& as() const;
    template <> const bool& as<bool>() const {
        if (fields.repr_ix != BOOL_I) {
            if (fields.repr_ix == DSRC_I) {
                return dsrc_read().as<bool>();
            }
            throw wrong_type(fields.repr_ix, BOOL_I);
        }
        return repr.b;
    }

    template <> const Int& as<Int>() const {
        if (fields.repr_ix != INT_I) {
            if (fields.repr_ix == DSRC_I) {
                return dsrc_read().as<Int>();
            }
            throw wrong_type(fields.repr_ix, INT_I);
        }
        return repr.i;
    }

    template <> const UInt& as<UInt>() const {
        if (fields.repr_ix != UINT_I) {
            if (fields.repr_ix == DSRC_I) {
                return dsrc_read().as<UInt>();
            }
            throw wrong_type(fields.repr_ix, UINT_I);
        }
        return repr.u;
    }

    template <> const Float& as<Float>() const {
        if (fields.repr_ix != FLOAT_I) {
            if (fields.repr_ix == DSRC_I) {
                return dsrc_read().as<Float>();
            }
            throw wrong_type(fields.repr_ix, FLOAT_I);
        }
        return repr.f;
    }

    template <> const String& as<String>() const {
        if (fields.repr_ix != STR_I) {
            if (fields.repr_ix == DSRC_I) {
                return dsrc_read().as<String>();
            }
            throw wrong_type(fields.repr_ix, STR_I);
        }
        return std::get<0>(*repr.ps);
    }

    bool is_empty() const;
    bool is_null() const;
    bool is_bool() const;
    bool is_int() const;;
    bool is_uint() const;;
    bool is_float() const;;
    bool is_str() const;;
    bool is_num() const;
    bool is_list() const;;
    bool is_map() const;;
    bool is_container() const;

    bool to_bool() const;
    Int to_int() const;
    UInt to_uint() const;
    Float to_float() const;
    std::string to_str() const;
    Key to_key() const;
    Key swap_key();
    std::string to_json() const;

    Object operator [] (is_integral auto v) const        { return get(v); }
    Object operator [] (const char* v) const             { return get(v); }
    Object operator [] (const std::string_view& v) const { return get(v); }
    Object operator [] (const std::string& v) const      { return get(v); }
    Object operator [] (bool v) const                    { return get(v); }
    Object operator [] (const Key& key) const            { return get(key); }
    Object operator [] (const Object& obj) const         { return get(obj); }

    Object get(is_integral auto v) const;
    Object get(const char* v) const;
    Object get(const std::string_view& v) const;
    Object get(const std::string& v) const;
    Object get(bool v) const;
    Object get(const Key& key) const;
    Object get(const Object& obj) const;

    size_t size() const;

    bool operator == (const Object&) const;
    std::partial_ordering operator <=> (const Object&) const;

    Oid id() const;
    bool is(const Object& other) const { return id() == other.id(); }
    refcnt_t ref_count() const;
    void inc_ref_count() const;
    void dec_ref_count() const;
    void free();

    // OTHER
    Object& operator = (const Object& other);
    Object& operator = (Object&& other);
    // NEED other assignment ops to avoid Object temporaries

  private:
    int repr_ix() const;
    Object dsrc_read() const;

    void set_reference(const Object& object);
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

  friend class WalkDF;
  friend class WalkBF;
};


class ISource
{
  public:
    virtual ~ISource() {}

    // this operation may throw a WrongType exception, if it's a partial map
    virtual Object read() = 0;
    virtual Object read_key(const Key&) = 0;
    virtual void write(const Object&) = 0;
    virtual void write(Object&&) = 0;
    virtual size_t size() = 0;
    virtual Object::ReprType type() = 0;

    virtual void reset() = 0;
    virtual void refresh() = 0;

    Object get_parent() const           { return parent; }
    void set_parent(Object& new_parent) { parent.free(); parent = new_parent; }

    refcnt_t ref_count() const { return rc; }
    void inc_ref_count()       { rc++; }
    refcnt_t dec_ref_count()   { return --rc; }

  private:
    Object parent;
    refcnt_t rc = 1;
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
        case MAP_I:   other.inc_ref_count(); repr.pm = other.repr.pm; break;
        case DSRC_I:  repr.ds->inc_ref_count(); repr.ds = other.repr.ds; break;
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
        case MAP_I:   repr.pm = other.repr.pm; break;
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
Object::Object(Map&& map) : repr{new IRCMap(std::move(map), NoParent{})}, fields{MAP_I} {
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
const Object Object::parent() const {
    return const_cast<Object*>(this)->parent();
}

inline
Object Object::parent() {
    switch (fields.repr_ix) {
        case EMPTY_I: throw empty_reference(__FUNCTION__);
        case STR_I:   return std::get<1>(*repr.ps);
        case LIST_I:  return std::get<1>(*repr.pl);
        case MAP_I:   return std::get<1>(*repr.pm);
        case DSRC_I:  return repr.ds->get_parent();
        default:      return nullptr;
    }
}

template <typename V>
void Object::visit(V&& visitor) const {
    switch (fields.repr_ix) {
        case EMPTY_I: throw wrong_type(EMPTY_I);
        case NULL_I:  visitor(nullptr); break;
        case BOOL_I:  visitor(repr.b); break;
        case INT_I:   visitor(repr.i); break;
        case UINT_I:  visitor(repr.u); break;
        case FLOAT_I: visitor(repr.f); break;
        case STR_I:   visitor(std::get<0>(*repr.ps)); break;
        case DSRC_I:  repr.ds->read().visit(std::forward<V>(visitor)); break;
        default:      throw wrong_type(fields.repr_ix);
    }
}


inline
bool Object::is_empty() const {
    return fields.repr_ix == EMPTY_I;
}

inline
bool Object::is_null() const {
    if (fields.repr_ix != NULL_I) {
        if (fields.repr_ix == DSRC_I)
            return repr.ds->read().is_null();
        return false;
    }
    return true;
}

inline
bool Object::is_bool() const {
    if (fields.repr_ix != BOOL_I) {
        if (fields.repr_ix == DSRC_I)
            return repr.ds->read().is_bool();
        return false;
    }
    return true;
}

inline
bool Object::is_int() const {
    if (fields.repr_ix != INT_I) {
        if (fields.repr_ix == DSRC_I)
            return repr.ds->read().is_int();
        return false;
    }
    return true;
}

inline
bool Object::is_uint() const {
    if (fields.repr_ix != UINT_I) {
        if (fields.repr_ix == DSRC_I)
            return repr.ds->read().is_uint();
        return false;
    }
    return true;
}

inline
bool Object::is_float() const {
    if (fields.repr_ix != FLOAT_I) {
        if (fields.repr_ix == DSRC_I)
            return repr.ds->read().is_float();
        return false;
    }
    return true;
}

inline
bool Object::is_str() const {
    if (fields.repr_ix != STR_I) {
        if (fields.repr_ix == DSRC_I)
            return repr.ds->read().is_str();
        return false;
    }
    return true;
}

inline
bool Object::is_num() const {
    int repr_ix = fields.repr_ix;
    if (repr_ix == DSRC_I) {
        repr_ix = const_cast<ISourcePtr>(repr.ds)->read().fields.repr_ix;
    }
    return repr_ix == INT_I || repr_ix == UINT_I || repr_ix == FLOAT_I;
}

inline
bool Object::is_list() const {
    if (fields.repr_ix != LIST_I) {
        if (fields.repr_ix == DSRC_I)
            return repr.ds->read().is_list();
        return false;
    }
    return true;
}

inline
bool Object::is_map() const {
    if (fields.repr_ix != MAP_I) {
        if (fields.repr_ix == DSRC_I)
            return repr.ds->read().is_map();
        return false;
    }
    return true;
}

inline
bool Object::is_container() const {
    int repr_ix = fields.repr_ix;
    if (repr_ix == DSRC_I) {
        repr_ix = const_cast<ISourcePtr>(repr.ds)->read().fields.repr_ix;
    }
    return repr_ix == LIST_I || repr_ix == MAP_I;
}

inline
int Object::repr_ix() const {
    return (fields.repr_ix == DSRC_I)? const_cast<ISourcePtr>(repr.ds)->type(): fields.repr_ix;
}

inline
Object Object::dsrc_read() const {
    return const_cast<ISourcePtr>(repr.ds)->read();
}

inline
void Object::set_reference(const Object& object) {
    free();
    (*this) = object;
}

inline
void Object::set_parent(const Object& new_parent) {
    switch (fields.repr_ix) {
        case STR_I: {
            auto& parent = std::get<1>(*repr.ps);
            parent.free();
            parent = new_parent;
            break;
        }
        case LIST_I: {
            auto& parent = std::get<1>(*repr.pl);
            parent.free();
            parent = new_parent;
            break;
        }
        case MAP_I: {
            auto& parent = std::get<1>(*repr.pm);
            parent.free();
            parent = new_parent;
            break;
        }
        case DSRC_I: {
            auto parent = repr.ds->get_parent();
            parent.free();
            parent = new_parent;
            break;
        }
        default:
            break;
    }
}

template <typename T>
T Object::value_cast() const {
    switch (fields.repr_ix) {
        case BOOL_I:  return (T)repr.b;
        case INT_I:   return (T)repr.i;
        case UINT_I:  return (T)repr.u;
        case FLOAT_I: return (T)repr.f;
        case DSRC_I:  return repr.ds->read().value_cast<T>();
        default:
            throw wrong_type(fields.repr_ix);
    }
}

template <typename T>
const T& Object::as() const {
    switch (fields.repr_ix) {
        case BOOL_I:  return repr.b;
        case INT_I:   return repr.i;
        case UINT_I:  return repr.u;
        case FLOAT_I: return repr.f;
        case DSRC_I:  return const_cast<ISourcePtr>(repr.ds)->read().as<T>();
        default:
            throw wrong_type(fields.repr_ix);
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
        case MAP_I:
            return to_json();
        case DSRC_I:
            return const_cast<ISourcePtr>(repr.ds)->read().to_str();
        default:
            throw std::logic_error("Unknown representation index");
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
        case STR_I:
        case LIST_I:
        case MAP_I:
            throw wrong_type(fields.repr_ix, BOOL_I);
        case DSRC_I:
            return const_cast<ISourcePtr>(repr.ds)->read().to_bool();
        default:
            throw std::logic_error("Unknown representation index");
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
        case STR_I:
        case LIST_I:
        case MAP_I:
            throw wrong_type(fields.repr_ix, INT_I);
        case DSRC_I:
            return const_cast<ISourcePtr>(repr.ds)->read().to_int();
        default:
            throw std::logic_error("Unknown representation index");
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
        case STR_I:
        case LIST_I:
        case MAP_I:
            throw wrong_type(fields.repr_ix, UINT_I);
        case DSRC_I:
            return const_cast<ISourcePtr>(repr.ds)->read().to_uint();
        default:
            throw std::logic_error("Unknown representation index");
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
        case STR_I:
        case LIST_I:
        case MAP_I:
            throw wrong_type(fields.repr_ix, FLOAT_I);
        case DSRC_I:
            return const_cast<ISourcePtr>(repr.ds)->read().to_float();
        default:
            throw std::logic_error("Unknown representation index");
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
        case LIST_I:
        case MAP_I:
            throw wrong_type(fields.repr_ix);
        case DSRC_I:
            return const_cast<ISourcePtr>(repr.ds)->read().to_key();
        default:
            throw wrong_type(fields.repr_ix);
    }
}

inline
Key Object::swap_key() {
    switch (fields.repr_ix) {
        case EMPTY_I: throw empty_reference(__FUNCTION__);
        case NULL_I:  { return nullptr; }
        case BOOL_I:  { Key k{repr.b}; repr.z = nullptr; fields.repr_ix = EMPTY_I; return k; }
        case INT_I:   { Key k{repr.i}; repr.z = nullptr; fields.repr_ix = EMPTY_I; return k; }
        case UINT_I:  { Key k{repr.u}; repr.z = nullptr; fields.repr_ix = EMPTY_I; return k; }
        case FLOAT_I: { Key k{repr.f}; repr.z = nullptr; fields.repr_ix = EMPTY_I; return k; }
        case STR_I:   {
            Key k{std::move(std::get<0>(*repr.ps))};
            repr.z = nullptr;
            fields.repr_ix = EMPTY_I;
            return k;
        }
        case LIST_I:
        case MAP_I:
            throw wrong_type(fields.repr_ix);
        case DSRC_I:
            return repr.ds->read().swap_key();
        default:
            throw wrong_type(fields.repr_ix);
    }
}

inline
Object Object::get(const char* v) const {
    switch (fields.repr_ix) {
        case EMPTY_I: throw empty_reference(__FUNCTION__);
        case MAP_I:   return std::get<0>(*repr.pm).at(v);
        case DSRC_I:  return repr.ds->read_key(Key{v});
        default:      throw wrong_type(fields.repr_ix);
    }
}

inline
Object Object::get(const std::string_view& v) const {
    switch (fields.repr_ix) {
        case EMPTY_I: throw empty_reference(__FUNCTION__);
        case MAP_I:   return std::get<0>(*repr.pm).at(v);
        case DSRC_I:  return repr.ds->read_key(Key{v});
        default:      throw wrong_type(fields.repr_ix);
    }
}

inline
Object Object::get(const std::string& v) const {
    switch (fields.repr_ix) {
        case EMPTY_I: throw empty_reference(__FUNCTION__);
        case MAP_I:   return std::get<0>(*repr.pm).at((std::string_view)v);
        case DSRC_I:  return repr.ds->read_key(Key{v});
        default:      throw wrong_type(fields.repr_ix);
    }
}

inline
Object Object::get(is_integral auto index) const {
    switch (fields.repr_ix) {
        case EMPTY_I: throw empty_reference(__FUNCTION__);
        case LIST_I: {
            List& list = std::get<0>(*repr.pl);
            return (index < 0)? list[list.size() + index]: list[index];
        }
        case MAP_I:  return std::get<0>(*repr.pm).at(index);
        case DSRC_I: return repr.ds->read_key(Key{index});
        default:
            throw wrong_type(fields.repr_ix);
    }
}

inline
Object Object::get(bool v) const {
    switch (fields.repr_ix) {
        case EMPTY_I: throw empty_reference(__FUNCTION__);
        case MAP_I:   return std::get<0>(*repr.pm).at(v);
        case DSRC_I: return repr.ds->read_key(Key{v});
        default:      throw wrong_type(fields.repr_ix);
    }
}

inline
Object Object::get(const Key& key) const {
    switch (fields.repr_ix) {
        case EMPTY_I: throw empty_reference(__FUNCTION__);
        case LIST_I: {
            List& list = std::get<0>(*repr.pl);
            Int index = key.to_int();
            return (index < 0)? list[list.size() + index]: list[index];
        }
        case MAP_I:  return std::get<0>(*repr.pm).at(key);
        case DSRC_I: return repr.ds->read_key(key);
        default:
            throw wrong_type(fields.repr_ix);
    }
}

inline
Object Object::get(const Object& obj) const {
    switch (fields.repr_ix) {
        case EMPTY_I: throw empty_reference(__FUNCTION__);
        case LIST_I: {
            List& list = std::get<0>(*repr.pl);
            Int index = obj.to_int();
            return (index < 0)? list[list.size() + index]: list[index];
        }
        case MAP_I:
        case DSRC_I: {
            // make temporary key without copy
            Key key;
            switch (obj.fields.repr_ix) {
                case NULL_I:  break;
                case BOOL_I:  key = obj.repr.b; break;
                case INT_I:   key = obj.repr.i; break;
                case UINT_I:  key = obj.repr.u; break;
                case FLOAT_I: key = obj.repr.b; break;
                case STR_I:   key = (std::string_view)std::get<0>(*obj.repr.ps); break;
                case LIST_I:
                case MAP_I:
                default:
                    throw wrong_type(fields.repr_ix);
            }
            return (fields.repr_ix == DSRC_I)? repr.ds->read_key(key): std::get<0>(*repr.pm).at(key);
        }
        default:
            throw wrong_type(fields.repr_ix);
    }
}

inline
size_t Object::size() const {
    switch (fields.repr_ix) {
        case EMPTY_I: throw empty_reference(__FUNCTION__);
        case STR_I:   return std::get<0>(*repr.ps).size();
        case LIST_I:  return std::get<0>(*repr.pl).size();
        case MAP_I:   return std::get<0>(*repr.pm).size();
        case DSRC_I:  return repr.ds->size();
        default:
            return 0;
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
        case DSRC_I:
            return const_cast<ISourcePtr>(repr.ds)->read() == obj;
        default:
            throw wrong_type(fields.repr_ix);
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
        case DSRC_I:
            return const_cast<ISourcePtr>(repr.ds)->read() <=> obj;
        default:
            throw wrong_type(fields.repr_ix);
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
                    case Object::MAP_I: {
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
                        stack.emplace(parent, key, object.repr.ds->read(), FIRST_VALUE);
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
                case Object::MAP_I: {
                    auto& map = std::get<0>(*object.repr.pm);
                    for (auto it = map.cbegin(); it != map.cend(); it++) {
                        auto& [key, child] = *it;
                        deque.emplace_back(object, key, child);
                    }
                    break;
                }
                case Object::DSRC_I: {
                    deque.emplace_front(parent, key, object.repr.ds->read());
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
            case MAP_I:   ss << ((event & WalkDF::BEGIN_PARENT)? '{': '}'); break;
            case DSRC_I:
                throw wrong_type(DSRC_I);
            default:
                throw std::logic_error("Unknown representation index");
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
        case MAP_I:   return Oid{7, (uint64_t)(&(std::get<0>(*repr.pm)))};
        case DSRC_I:  return Oid{8, (uint64_t)(repr.ds)};
        default:
            throw std::logic_error("Unknown representation index");
    }
}

inline
refcnt_t Object::ref_count() const {
    switch (fields.repr_ix) {
        case STR_I:   return std::get<1>(*repr.ps).fields.ref_count;
        case LIST_I:  return std::get<1>(*repr.pl).fields.ref_count;
        case MAP_I:   return std::get<1>(*repr.pm).fields.ref_count;
        case DSRC_I:  return repr.ds->ref_count();
        default:
            return no_ref_count;
    }
}

inline
void Object::inc_ref_count() const {
    Object& self = *const_cast<Object*>(this);
    switch (fields.repr_ix) {
        case EMPTY_I: throw empty_reference(__FUNCTION__);
        case STR_I:   (std::get<1>(*self.repr.ps).fields.ref_count)++; break;
        case LIST_I:  (std::get<1>(*self.repr.pl).fields.ref_count)++; break;
        case MAP_I:   (std::get<1>(*self.repr.pm).fields.ref_count)++; break;
        case DSRC_I:  repr.ds->inc_ref_count(); break;
        default:      break;
    }
}

inline
void Object::dec_ref_count() const {
    assert (ref_count() != 0);  // TODO: remove later
    Object& self = *const_cast<Object*>(this);
    switch (fields.repr_ix) {
        case STR_I:   if (--(std::get<1>(*self.repr.ps).fields.ref_count) == 0) delete self.repr.ps; break;
        case LIST_I:  if (--(std::get<1>(*self.repr.pl).fields.ref_count) == 0) delete self.repr.pl; break;
        case MAP_I:   if (--(std::get<1>(*self.repr.pm).fields.ref_count) == 0) delete self.repr.pm; break;
        case DSRC_I:  if (repr.ds->dec_ref_count() == 0) delete self.repr.ds; break;
        default:      break;
    }
}

inline
void Object::free() {
    dec_ref_count();
    repr.z = nullptr;
    fields.repr_ix = EMPTY_I;
}

inline
void Object::set_children_parent(IRCList& irc_list) {
    Object& self = *this;
    List& list = std::get<0>(irc_list);
    for (auto& obj : list)
        obj.set_parent(self);
}

inline
void Object::set_children_parent(IRCMap& irc_map) {
    Map& map = std::get<0>(irc_map);
    for (auto& [key, obj] : map)
        const_cast<Object&>(obj).set_parent(*this);
}

inline
void Object::clear_children_parent(IRCList& irc_list) {
    List& list = std::get<0>(irc_list);
    for (auto& obj : list)
        obj.set_parent(null);
}

inline
void Object::clear_children_parent(IRCMap& irc_map) {
    Map& map = std::get<0>(irc_map);
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
            case MAP_I: {
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
                case MAP_I: {
                    clear_children_parent(*repr.pm);
                    std::get<0>(*repr.pm) = std::get<0>(*other.repr.pm);
                    set_children_parent(*repr.pm);
                    break;
                }
                case DSRC_I: {
                    repr.ds->write(other);
                    break;
                }
            }
        }
    }
    else
    {
        Object curr_parent{parent()};

        switch (fields.repr_ix) {
            case STR_I:  dec_ref_count(); break;
            case LIST_I: clear_children_parent(*repr.pl); dec_ref_count(); break;
            case MAP_I:  clear_children_parent(*repr.pm); dec_ref_count(); break;
            default:     break;
        }

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
                List& list = std::get<0>(*other.repr.pl);
                repr.pl = new IRCList{list, curr_parent};
                set_children_parent(*repr.pl);
                break;
            }
            case MAP_I: {
                Map& map = std::get<0>(*other.repr.pm);
                repr.pm = new IRCMap{map, curr_parent};
                set_children_parent(*repr.pm);
                break;
            }
            case DSRC_I: {
                repr.ds->write(other);
                break;
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
            case MAP_I:   repr.pm = other.repr.pm; break;
            case DSRC_I:   repr.ds = other.repr.ds; break;
        }
    }
    else if (fields.repr_ix == other.fields.repr_ix)
    {
        if (!is(other)) {
            fields.repr_ix = other.fields.repr_ix;
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
                case MAP_I: {
                    clear_children_parent(*repr.pm);
                    std::get<0>(*repr.pm) = std::get<0>(*other.repr.pm);
                    set_children_parent(*repr.pm);
                    break;
                }
                case DSRC_I: {
                    repr.ds->write(other);
                    break;
                }
            }
        }
    }
    else
    {
        Object curr_parent{parent()};

        switch (fields.repr_ix) {
            case STR_I:  dec_ref_count(); break;
            case LIST_I: clear_children_parent(*repr.pl); dec_ref_count(); break;
            case MAP_I:  clear_children_parent(*repr.pm); dec_ref_count(); break;
            default:     break;
        }

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
                List& list = std::get<0>(*other.repr.pl);
                repr.pl = new IRCList{std::move(list), curr_parent};
                set_children_parent(*repr.pl);
                break;
            }
            case MAP_I: {
                Map& map = std::get<0>(*other.repr.pm);
                repr.pm = new IRCMap{std::move(map), curr_parent};
                set_children_parent(*repr.pm);
                break;
            }
            case DSRC_I: {
                repr.ds->write(other);
                break;
            }
        }
    }

    other.fields.repr_ix = EMPTY_I;
    other.repr.z = nullptr; // safe, but may not be necessary

    return *this;
}

} // nodel namespace
