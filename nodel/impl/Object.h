#pragma once

#include <limits>
#include <string>
#include <vector>
#include <tsl/ordered_map.h>
#include <stack>
#include <deque>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <variant>
#include <functional>
#include <fmt/core.h>

#include "support.h"
#include "Oid.h"

namespace nodel {

class Object;
class Node;

using Int = int64_t;
using UInt = uint64_t;
using Float = double;

template<typename T>
concept is_like_Int = std::is_signed<T>::value && std::is_integral<T>::value && std::is_convertible_v<T, Int>;

template<typename T>
concept is_like_UInt = std::is_unsigned<T>::value && std::is_integral<T>::value && std::is_convertible_v<T, UInt>;

template<typename T>
concept is_integral = std::is_integral<T>::value;

template<typename T>
concept is_like_Float = std::is_floating_point<T>::value;

struct Key
{
    using DatumType = std::variant<
        void*,  // null key
        bool,
        Int,
        UInt,
        Float,
//        const char*,
        std::string
    >;

    Key() : dat{(void*)0} {}

    Key(const char* v) : dat{std::string(v)} {}
    Key(const std::string& v) : dat{v} {}
    Key(bool v) : dat{v} {}
    Key(is_like_Float auto v) : dat{(Float)v} {}
    Key(is_like_Int auto v) : dat{(Int)v} {}
    Key(is_like_UInt auto v) : dat{(UInt)v} {}

    Key(const Key& key) : dat{key.dat} {}
    Key(Key&& key) : dat{std::move(key.dat)} {}
    Key& operator = (const Key&) = default;
    Key& operator = (Key&&) = default;

    Key& operator = (auto&& v) { dat = v; return *this; }

    bool operator == (const Key& other) const { return dat == other.dat; }
    bool operator == (const std::string& other) const { return to_str() == other; }
    bool operator == (const char* other) const { return to_str() == other; }
    bool operator == (auto other) const {
        bool rv;
        std::visit(overloaded {
            [] (void*) { throw std::bad_variant_access{}; },
            [&rv, &other] (auto v)       { rv = (v == other); },
//            [&rv] (const char* s)        { rv = false; },
            [&rv] (const std::string& v) { rv = false; }
        }, dat);
        return rv;
    }

    template <typename T>
    bool is_type() const {
      return std::holds_alternative<T>(dat);
    }

    bool is_null() const  { return is_type<void*>(); }
    bool is_bool() const  { return is_type<bool>(); }
    bool is_int() const   { return is_type<Int>(); };
    bool is_uint() const  { return is_type<UInt>(); };
    bool is_float() const { return is_type<Float>(); };
    bool is_str() const   { return is_type<std::string>(); };
    bool is_num() const   { return is_bool() || is_int() || is_uint() || is_float(); }

    bool as_bool() const       { return std::get<bool>(dat); }
    Int as_int() const         { return std::get<Int>(dat); }
    UInt as_uint() const       { return std::get<UInt>(dat); }
    Float as_float() const     { return std::get<Float>(dat); }
    std::string as_str() const { return std::get<std::string>(dat); }

    Int to_int() const {
        Int rv;
        std::visit(overloaded {
            [&rv] (bool v) { rv = (Int)v; },
            [&rv] (Int v) { rv = v; },
            [&rv] (UInt v) { rv = (Int)v; },
            [] (auto&&) { throw std::bad_variant_access(); }
        }, dat);
        return rv;
    }

    UInt to_uint() const {
        UInt rv;
        std::visit(overloaded {
            [&rv] (bool v) { rv = (UInt)v; },
            [&rv] (Int v) { rv = (UInt)v; },
            [&rv] (UInt v) { rv = v; },
            [] (auto&&) { throw std::bad_variant_access(); }
        }, dat);
        return rv;
    }

    std::string to_str() const {
        std::stringstream ss;
        std::visit(overloaded {
            [&ss] (void*) { ss << "null"; },
            [&ss] (const auto v) { ss << v; },
            [&ss] (const std::string& v) { ss << v; }
        }, dat);
        return ss.str();
    }

    std::string to_json() const {
        std::stringstream ss;
        std::visit(overloaded {
            [&ss] (void*) { ss << "null"; },
            [&ss] (const auto v) { ss << v; },
            [&ss] (const std::string& v) { ss << std::quoted(v); }
        }, dat);
        return ss.str();
    }

    DatumType dat;
};

struct KeyHash
{
    size_t operator () (const Key& key) const {
        std::hash<decltype(key.dat)> hash;
        return hash(key.dat);
    }
};


class Path;
class AbstractLoader;

using Map = tsl::ordered_map<Key, Object, KeyHash>;
using List = std::vector<Object>;

// inplace reference count types
using ref_count_t = uint32_t;
using IRCString = std::tuple<std::string, ref_count_t>;
using IRCList = std::tuple<List, ref_count_t>;
using IRCMap = std::tuple<Map, ref_count_t>;

using StringPtr = IRCString*;
using ListPtr = IRCList*;
using MapPtr = IRCMap*;

// Datum must have a minimal footprint since it is the primary data representation
using Datum = std::variant<
    void*,  // empty type (does not point to anything)
    bool,
    Int,
    UInt,
    Float,
    StringPtr,
    ListPtr,
    MapPtr
>;

enum {
    NULL_TYPE,
    BOOL_TYPE,
    INT_TYPE,
    UINT_TYPE,
    FLOAT_TYPE,
    STRING_TYPE,
    GENERIC_LIST_TYPE,
    GENERIC_MAP_TYPE
};

// NOTE: use by-value semantics
class Object
{
  public:
    static constexpr ref_count_t no_ref_count = std::numeric_limits<ref_count_t>::max();

    Object() : dat{(void*)0} {}

    Object(const char* v) : dat{new IRCString{v, 1}} {}
    Object(const std::string& v) : dat{new IRCString{v, 1}} {}
    Object(std::string&& str) : dat{new IRCString(std::move(str), 1)} {}
    Object(bool v) : dat{v} {}
    Object(is_like_Float auto v) : dat{(Float)v} {}
    Object(is_like_Int auto v) : dat{(Int)v} {}
    Object(is_like_UInt auto v) : dat{(UInt)v} {}

    Object(List& list) : dat{new IRCList(list, 1)} {}
    Object(List&& list) : dat{new IRCList(list, 1)} {}
    Object(Map& map) : dat{new IRCMap(map, 1)} {}
    Object(Map&& map) : dat{new IRCMap(map, 1)} {}

    Object(const Object& other) : dat{other.dat} { inc_ref_count(); }
    Object(Object&& other) { std::swap(dat, other.dat); }

    Object& operator = (const Object& other) {
        free();
        dat = other.dat;
        inc_ref_count();
        return *this;
    }

    Object& operator = (Object&& other) {
        free();
        dat = other.dat;
        other.dat = (void*)0;
        return *this;
    }

    ~Object() { free(); }

    template <typename T>
    bool is_type() const;

    template <typename T>
    T value_cast() const;

    template <typename T>
    const T& as() const;

    template <typename T>
    T& as();

    bool is_null() const      { return is_type<void*>(); }
    bool is_bool() const      { return is_type<bool>(); }
    bool is_int() const       { return is_type<Int>(); };
    bool is_uint() const      { return is_type<UInt>(); };
    bool is_float() const     { return is_type<Float>(); };
    bool is_str() const       { return is_type<StringPtr>(); };
    bool is_num() const       { return is_int() || is_uint() || is_float(); }
    bool is_list() const      { return is_type<ListPtr>(); }
    bool is_map() const       { return is_type<MapPtr>(); }
    bool is_container() const { return is_list() || is_map(); }

    Int& as_int()         { return as<Int>(); }
    UInt& as_uint()       { return as<UInt>(); }
    Float& as_fp()        { return as<Float>(); }
    std::string& as_str() { return std::get<0>(*as<StringPtr>()); }
    List& as_list()       { return std::get<0>(*as<ListPtr>()); }
    Map& as_map()         { return std::get<0>(*as<MapPtr>()); }

    Int as_int() const                { return as<Int>(); }
    UInt as_uint() const              { return as<UInt>(); }
    Float as_fp() const               { return as<Float>(); }
    std::string const& as_str() const { return std::get<0>(*as<StringPtr>()); }
    List const& as_list() const       { return std::get<0>(*as<ListPtr>()); }
    Map const& as_map() const         { return std::get<0>(*as<MapPtr>()); }

    bool to_bool() const { return value_cast<bool>(); }
    Int to_int() const   { return value_cast<Int>(); }
    UInt to_uint() const { return value_cast<UInt>(); }
    Float to_fp() const  { return value_cast<Float>(); }
    std::string to_str() const;
    Key to_key() const;
    std::string to_json() const;

    // TODO: simplify with auto
    const Object operator [] (is_integral auto v) const   { return get(v); }
    const Object operator [] (const char* v) const        { return get(v); }
    const Object operator [] (const std::string& v) const { return get(v); }
    const Object operator [] (bool v) const               { return get(v); }
    const Object operator [] (const Key& key) const       { return get(key); }
    const Object operator [] (const Object& obj) const    { return get(obj); }

    Object operator [] (is_integral auto v)     { return get(v); }
    Object operator [] (const char* v)         { return get(v); }
    Object operator [] (const std::string& v)  { return get(v); }
    Object operator [] (bool v)                { return get(v); }
    Object operator [] (const Key& key)        { return get(key); }
    Object operator [] (const Object& obj)     { return get(obj); }

    const Object get(is_integral auto v) const   { return const_cast<Object*>(this)->get(v); }
    const Object get(const char* v) const        { return const_cast<Object*>(this)->get(v); }
    const Object get(const std::string& v) const { return const_cast<Object*>(this)->get(v); }
    const Object get(bool v) const               { return const_cast<Object*>(this)->get(v); }
    const Object get(const Key& key) const       { return const_cast<Object*>(this)->get(key); }
    const Object get(const Object& obj) const    { return const_cast<Object*>(this)->get(obj); }

    Object get(is_integral auto v);
    Object get(const char* v);
    Object get(const std::string& v);
    Object get(bool v);
    Object get(const Key& key);
    Object get(const Object& obj);

    bool operator == (const Object&) const;
    auto operator <=> (const Object&) const;

    Oid id() const;
    size_t hash() const;
    ref_count_t ref_count() const;

    friend class WalkDF;
    friend class WalkBF;

    void inc_ref_count() const;
    void dec_ref_count() const;

  private:
    void free() {
      // TODO: instead of immediately releasing memory, queue this object to
      // a different thread for garbage collection.
      dec_ref_count();
      dat = (void*)0;
    }

  private:
    Datum dat;

  friend class Node;
  friend class NodeAccess;
};


template <typename T>
bool Object::is_type() const {
    return std::holds_alternative<T>(dat);
}

template <typename T>
const T& Object::as() const {
    return std::get<T>(dat);
}

template <typename T>
T& Object::as() {
    return std::get<T>(dat);
}

template <typename T>
T Object::value_cast() const {
    T rv;
    std::visit(overloaded {
      [] (auto&& v) { throw std::bad_variant_access(); },
      [] (void*) { throw std::bad_variant_access(); },
      [&rv] (bool v) { rv = (T)v; },
      [&rv] (Int v) { rv = (T)v; },
      [&rv] (UInt v) { rv = (T)v; },
      [&rv] (Float v) { rv = (T)v; },
      [] (StringPtr ptr) { throw std::bad_variant_access(); },
      [] (ListPtr ptr) { throw std::bad_variant_access(); },
      [] (MapPtr ptr) { throw std::bad_variant_access(); }
    }, dat);
    return rv;
}

inline
std::string Object::to_str() const {
  std::stringstream ss;
  std::visit(overloaded {
    [&ss] (auto&& v) { ss << "?"; },
    [&ss] (void*) { ss << "null"; },
    [&ss] (bool v) { ss << v; },
    [&ss] (Int v) { ss << v; },
    [&ss] (UInt v) { ss << v; },
    [&ss] (Float v) { ss << v; },
    [&ss] (StringPtr p) { ss << std::get<0>(*p); },
    [this, &ss] (ListPtr p) { ss << to_json(); },
    [this, &ss] (MapPtr p) { ss << to_json(); }
  }, dat);
  return ss.str();
}

inline
Object Object::get(const char* v) {
    auto ptr = std::get_if<MapPtr>(&dat);
    if (ptr != nullptr) return std::get<0>(**ptr).at(Key{v});
    throw std::bad_variant_access();
}

inline
Object Object::get(const std::string& v) {
    return get(v.c_str());
}

inline
Object Object::get(is_integral auto index) {
    switch (dat.index()) {
        case GENERIC_LIST_TYPE: {
            List& list = std::get<0>(*std::get<ListPtr>(dat));
            return (index < 0)? list[list.size() + index]: list[index];
        }
        case GENERIC_MAP_TYPE: {
            return std::get<0>(*std::get<MapPtr>(dat))[Key{index}];
        }
        default:
            throw std::bad_variant_access();
    }
}

inline
Object Object::get(bool v) {
    auto ptr = std::get_if<MapPtr>(&dat);
    if (ptr != nullptr) return std::get<0>(**ptr).at(Key{v});
    throw std::bad_variant_access();
}

inline
Object Object::get(const Key& key) {
    switch (dat.index()) {
        case GENERIC_LIST_TYPE: {
            List& list = std::get<0>(*std::get<ListPtr>(dat));
            Int index = key.to_int();
            return (index < 0)? list[list.size() + index]: list[index];
        }
        case GENERIC_MAP_TYPE: {
            return std::get<0>(*std::get<MapPtr>(dat))[key];
        }
        default:
            throw std::bad_variant_access();
    }
}

inline
Object Object::get(const Object& obj) {
    switch (dat.index()) {
        case GENERIC_LIST_TYPE: {
            List& list = std::get<0>(*std::get<ListPtr>(dat));
            Int index = obj.to_int();
            return (index < 0)? list[list.size() + index]: list[index];
        }
        case GENERIC_MAP_TYPE: {
            return std::get<0>(*std::get<MapPtr>(dat))[obj.to_key()];
        }
        default:
            throw std::bad_variant_access();
    }
}

inline
bool Object::operator == (const Object& obj) const {
    bool rv;
    std::visit(overloaded {
        [&rv] (auto&&) { rv = false; },
        [] (void *) { throw std::bad_variant_access(); },
        [&obj, &rv] (bool lhs) { rv = (lhs == obj.to_bool()); },
        [&obj, &rv] (Int lhs) { rv = (lhs == obj.to_int()); },
        [&obj, &rv] (UInt lhs) { rv = (lhs == obj.to_uint()); },
        [&obj, &rv] (Float lhs) { rv = (lhs == obj.to_fp()); },
        [&obj, &rv] (StringPtr lhs) { rv = std::get<0>(*lhs) == obj.as_str(); },
        [] (ListPtr) { throw std::bad_variant_access(); },
        [] (MapPtr) { throw std::bad_variant_access(); }
    }, dat);
    return rv;
}

inline
auto Object::operator <=> (const Object& obj) const {
    int rv;

    std::visit(overloaded {
        [] (void *) { throw std::bad_variant_access(); },
        [&obj, &rv] (auto&& lhs) {
            std::visit(overloaded {
                [] (void*) { throw std::bad_variant_access(); },
                [lhs, &rv] (auto&& rhs) { if (lhs < rhs) rv = -1; else if (lhs > rhs) rv = 1; else rv = 0; },
                [&obj, &rv] (StringPtr p_rhs) { rv = obj.as_str().compare(std::get<0>(*p_rhs)); },
                [] (ListPtr) { throw std::bad_variant_access(); },
                [] (MapPtr) { throw std::bad_variant_access(); }
            }, obj.dat);
        },
        [&obj, &rv] (StringPtr p_lhs) { rv = std::get<0>(*p_lhs).compare(obj.as_str()); },
        [] (ListPtr p) { throw std::bad_variant_access(); },
        [] (MapPtr p) { throw std::bad_variant_access(); }
    }, dat);

    return rv;
}

inline
Key Object::to_key() const {
    Key key;
    std::visit(overloaded {
        [] (auto&&) { throw std::bad_variant_access(); },
        [] (void* v) { throw std::bad_variant_access(); },
        [&key] (bool v) { key = (Int)v; },
        [&key] (Int v) { key = v; },
        [&key] (UInt v) { key = v; },
        [&key] (Float v) { key = v; },
        [&key] (StringPtr ptr) { key = std::get<0>(*ptr); },
        [] (ListPtr) { throw std::bad_variant_access(); },
        [] (MapPtr) { throw std::bad_variant_access(); }
    }, dat);
    return key;
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
        stack.emplace(Object(), 0, root, FIRST_VALUE);
    }

    bool next() {
        bool has_next = !stack.empty();
        if (has_next) {
            const auto item = stack.top();
            stack.pop();

            if (std::get<3>(item) & END_PARENT) {
                auto& [parent, key, object, event] = item;
                visitor(parent, key, object, event);
            } else {
                std::visit(overloaded {
                    [this, &item] (auto&& v) {
                        auto& [parent, key, object, event] = item;
                        visitor(parent, key, object, event);
                    },
                    [this, &item] (const ListPtr ptr) {
                        auto& [parent, key, object, event] = item;
                        visitor(parent, key, object, event | BEGIN_PARENT);
                        stack.emplace(parent, key, object, event | END_PARENT);
                        auto& list = std::get<0>(*ptr);
                        Int index = list.size() - 1;
                        for (auto it = list.crbegin(); it != list.crend(); it++, index--) {
                            stack.emplace(object, index, *it, (index == 0)? FIRST_VALUE: NEXT_VALUE);
                        }
                    },
                    [this, &item] (const MapPtr ptr) {
                        auto& [parent, key, object, event] = item;
                        visitor(parent, key, object, event | BEGIN_PARENT);
                        stack.emplace(parent, key, object, event | END_PARENT);
                        auto& map = std::get<0>(*ptr);
                        Int index = map.size() - 1;
                        for (auto it = map.rcbegin(); it != map.rcend(); it++, index--) {
                            auto& [key, child] = *it;
                            stack.emplace(object, key, child, (index == 0)? FIRST_VALUE: NEXT_VALUE);
                        }
                    }
                }, std::get<2>(item).dat);
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
        deque.emplace_back(Object(), 0, root);
    }

    bool next() {
        bool has_next = !deque.empty();
        if (has_next) {
            const auto item = deque.front();
            deque.pop_front();

            std::visit(overloaded {
                [this, &item] (auto&& v) {
                    auto& [parent, key, object] = item;
                    visitor(parent, key, object);
                },
                [this, &item] (const ListPtr ptr) {
                    auto& [parent, key, object] = item;
                    auto& list = std::get<0>(*ptr);
                    Int index = 0;
                    for (auto it = list.cbegin(); it != list.cend(); it++, index++) {
                        const auto& child = *it;
                        deque.emplace_back(object, index, child);
                    }
                },
                [this, &item] (const MapPtr ptr) {
                    auto& [parent, key, object] = item;
                    auto& map = std::get<0>(*ptr);
                    for (auto it = map.cbegin(); it != map.cend(); it++) {
                        auto& [key, child] = *it;
                        deque.emplace_back(object, key, child);
                    }
                }
            }, std::get<2>(item).dat);
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
        auto visit_prim = overloaded {
            [&ss] (auto v) { ss.precision(10); ss << v; },
            [&ss] (void*) { ss << "null"; },
            [&ss] (bool v) { ss << (v? "true": "false"); },
            [&ss] (StringPtr p) { ss << nodel::quoted(std::get<0>(*p)); },
            [&ss, event] (ListPtr p) { ss << ((event & WalkDF::BEGIN_PARENT)? '[': ']'); },
            [&ss, event] (MapPtr p) { ss << ((event & WalkDF::BEGIN_PARENT)? '{': '}'); }
        };

        if (event & WalkDF::NEXT_VALUE && !(event & WalkDF::END_PARENT)) {
            ss << ", ";
        }

        if (parent.is_list()) {
            std::visit(visit_prim, object.dat);
        } else if (parent.is_map()) {
            if (!(event & WalkDF::END_PARENT))
                ss << key.to_json() << ": ";
            std::visit(visit_prim, object.dat);
        } else {
            std::visit(visit_prim, object.dat);
        }
    };

    WalkDF walk{*this, visitor};
    while (walk.next());

    return ss.str();
}

inline
Oid Object::id() const {
    static_assert(alignof(std::string) != 1);
    static_assert(alignof(IRCList) != 1);
    static_assert(alignof(IRCMap) != 1);

    Oid id;
    std::visit(overloaded {
        [&id] (auto&& v) { id = Oid::illegal(); },
        [&id] (void* p) { id = Oid::null(); },
        [&id] (bool v) { id = Oid{1, v}; },
        [&id] (Int v) { id = Oid{2, *((uint64_t*)(&v))}; },
        [&id] (UInt v) { id = Oid{3, v}; },
        [&id] (Float v) { id = Oid{4, *((uint64_t*)(&v))}; },
        [&id] (const StringPtr p) { id = Oid{5, (uint64_t)(&(std::get<0>(*p)))}; },
        [&id] (const ListPtr p) { id = Oid{5, (uint64_t)(&(std::get<0>(*p)))}; },
        [&id] (const MapPtr p) { id = Oid{5, (uint64_t)(&(std::get<0>(*p)))}; }
    }, dat);
    return id;
}

inline
size_t Object::hash() const {
    size_t hash = 0;
    std::visit(overloaded {
        [] (auto&& v) { throw std::bad_variant_access(); },
        [&hash] (void* p) { hash = 0; },
        [&hash] (bool v) { hash = (size_t)v; },
        [&hash] (Int v) { hash = *((size_t*)(&v)); },
        [&hash] (UInt v) { hash = *((size_t*)(&v)); },
        [&hash] (Float v) { hash = *((size_t*)(&v)); },
        [&hash] (const StringPtr p) { hash = std::hash<std::string>{}(std::get<0>(*p)); },
        [] (ListPtr) { throw std::bad_variant_access(); },
        [] (MapPtr) { throw std::bad_variant_access(); }
    }, dat);
    return hash;
}

inline
ref_count_t Object::ref_count() const {
    ref_count_t count = no_ref_count;
    std::visit(overloaded {
        [] (auto&& v) {},
        [&count] (const StringPtr p) { count = std::get<1>(*p); },
        [&count] (const ListPtr p) { count = std::get<1>(*p); },
        [&count] (const MapPtr p) { count = std::get<1>(*p);  }
    }, dat);
    return count;
}

inline
void Object::inc_ref_count() const {
    std::visit(overloaded {
        [] (auto&&) {},
        [] (StringPtr ptr) { std::get<1>(*ptr)++; },
        [] (ListPtr ptr) { std::get<1>(*ptr)++; },
        [] (MapPtr ptr) { std::get<1>(*ptr)++; }
    }, const_cast<Datum&>(dat));
}

inline
void Object::dec_ref_count() const {
    std::visit(overloaded {
        [] (auto&&) {},
        [] (StringPtr ptr) { if (--std::get<1>(*ptr) == 0) delete ptr; },
        [] (ListPtr ptr) { if (--std::get<1>(*ptr) == 0) delete ptr; },
        [] (MapPtr ptr) { if (--std::get<1>(*ptr) == 0) delete ptr; }
    }, const_cast<Datum&>(dat));
}

} // nodel namespace


// Omitting this implementation, because there are two possible implementations, and it's
// not appropriate to choose one of them, here.  The possibilities are hash by ID, and hash
// by value.
//template<>
//struct std::hash<nodel::Object>
//{
//    std::size_t operator () (const nodel::Object& obj) const noexcept {
//      return obj.hash();
//    }
//};
