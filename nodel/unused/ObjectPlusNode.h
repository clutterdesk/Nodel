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

#include "support.h"

namespace nodel {

class Object;
class Node;

using Int = int64_t;
using UInt = uint64_t;
using Float = double;

using Key = std::variant<
  Int,
  UInt,
  double,
  std::string
>;

inline
std::string key_to_str(const Key& key) {
  std::stringstream ss;
  std::visit(overloaded {
    [&ss] (const auto v) { ss << v; },
    [&ss] (const std::string& v) { ss << std::quoted(v); }
  }, key);
  return ss.str();
};

using Map = tsl::ordered_map<Key, Object>;
using List = std::vector<Object>;

using IRCString = std::tuple<std::string, size_t>;
using IRCList = std::tuple<List, size_t>;
using IRCMap = std::tuple<Map, size_t>;

using StringPtr = IRCString*;
using ListPtr = IRCList*;
using MapPtr = IRCMap*;

// Datum must have a minimal footprint since it is the primary data representation
using Datum = std::variant<
    void*,  // empty type (does not point to anything)
    bool,
    Int,
  UInt,
    double,
    StringPtr,
    ListPtr,
    MapPtr
>;

// NOTE: use by-value semantics
class Object
{
  public:
    Object() : dat{(void*)0} {}
    Object(auto v) : dat{v} {}
    Object(const std::string& str) : dat{new IRCString(str, 1)} {}
    Object(std::string&& str) : dat{new IRCString(str, 1)} {}
    Object(const char* str) : dat{new IRCString(str, 1)} {}
    Object(List& list) : dat{new IRCList(list, 1)} {}  // REMOVE, LATER
    Object(List&& list) : dat{new IRCList(list, 1)} {}  // REMOVE, LATER
    Object(Map& map) : dat{new IRCMap(map, 1)} {}  // REMOVE, LATER
    Object(Map&& map) : dat{new IRCMap(map, 1)} {}  // REMOVE, LATER

    Object(const Object& other) : dat{other.dat} {
        std::visit(overloaded {
            [] (auto&&) {},
            [] (StringPtr ptr) { std::get<1>(*ptr)++; },
            [] (ListPtr ptr) { std::get<1>(*ptr)++; },
            [] (MapPtr ptr) { std::get<1>(*ptr)++; }
        }, other.dat);
    }

    Object(Object&& other) : dat(other.dat) {
        other.dat = (void*)0;
    }

    ~Object() { free(); }

    static Object from_json(const std::string& json);

    Object& operator = (const Object& other) {
      free();

      dat = other.dat;
        std::visit(overloaded {
            [] (auto&&) {},
            [] (StringPtr ptr) { std::get<1>(*ptr)++; },
            [] (ListPtr ptr) { std::get<1>(*ptr)++; },
            [] (MapPtr ptr) { std::get<1>(*ptr)++; }
        }, other.dat);

        return *this;
    }

    Object& operator = (Object&& other) {
      free();
        dat = other.dat;
        other.dat = (void*)0;
        return *this;
    }

    bool is_null() const { return std::holds_alternative<void*>(dat); }
    bool is_bool() const { return std::holds_alternative<bool>(dat); }
    bool is_int() const { return std::holds_alternative<Int>(dat); };
    bool is_uint() const { return std::holds_alternative<UInt>(dat); };
    bool is_float() const { return std::holds_alternative<double>(dat); };
    bool is_string() const { return std::holds_alternative<StringPtr>(dat); };
    bool is_number() const { return is_int() || is_uint() || is_float(); }
    bool is_list() const { return std::holds_alternative<ListPtr>(dat); }
    bool is_map() const { return std::holds_alternative<MapPtr>(dat); }
    bool is_container() const { return is_list() || is_map(); }

    Int& as_int() { return std::get<Int>(dat); }
    Int as_int() const { return std::get<Int>(dat); }
    UInt& as_uint() { return std::get<UInt>(dat); }
    UInt as_uint() const { return std::get<UInt>(dat); }
    double& as_fp() { return std::get<double>(dat); }
    double as_fp() const { return std::get<double>(dat); }
    std::string& as_str() { return std::get<0>(*std::get<StringPtr>(dat)); }
    std::string const& as_str() const { return std::get<0>(*std::get<StringPtr>(dat)); }
//    List& as_list() { return std::get<0>(*std::get<ListPtr>(dat)); }
//    List const& as_list() const { return std::get<0>(*std::get<ListPtr>(dat)); }
//    Map& as_map() { return std::get<0>(*std::get<MapPtr>(dat)); }
//    Map const& as_map() const { return std::get<0>(*std::get<MapPtr>(dat)); }

    bool to_bool() const;
    Int to_int() const;
    UInt to_uint() const;
    Float to_fp() const;
    std::string to_str() const;
    Key to_key() const;
    std::string to_json() const;

    Node& operator [] (auto&&);  // numeric indices
    Node& operator [] (const char*);
    Node& operator [] (const Key&);
    Node& operator [] (const Object&);
    Node& operator [] (Key&&);
    Node& operator [] (Object&&);

    bool operator == (const Object&) const;
    auto operator <=> (const Object&) const;

    size_t ref_count() const;
    Int id() const;
    size_t hash() const;

    friend class WalkDF;
    friend class WalkBF;

  private:
    void free() {
      // TODO: instead of immediately releasing memory, queue this object to
      // a different thread for garbage collection.
        std::visit(overloaded {
            [] (auto&&) {},
            [] (StringPtr ptr) { if (--std::get<1>(*ptr) == 0) delete ptr; },
            [] (ListPtr ptr) { if (--std::get<1>(*ptr) == 0) delete ptr; },
            [] (MapPtr ptr) { if (--std::get<1>(*ptr) == 0) delete ptr; }
        }, dat);
        dat = (void*)0;
    }

  private:
    Datum dat;
};


class Node : public Object
{
  public:
  Node(const Node& node) : Object{node}, p_parent(node.p_parent) {}
  Node(Node&& node) : Object{node}, p_parent(node.p_parent) {}

  Node(const Object& object) : Object{object} {}
  Node(Object&& object) : Object{object} {}

  ~Node() = default;

  // Support assigning values between nodes
  Node& operator = (const Node& node) {
    Object& self = *this;
    Object& obj = node;
    self = obj;
    return *this;
  }

  // Support assigning values between nodes
  Node& operator = (Node&& node) {
    Object& self = *this;
    Object&& obj = node;
    self = obj;
    return *this;
  }

    Node& operator [] (auto&&);  // numeric indices
    Node& operator [] (const char*);
    Node& operator [] (const Key&);
    Node& operator [] (const Object&);
    Node& operator [] (Key&&);
    Node& operator [] (Object&&);

  private:
    Node* p_parent;
};


inline
bool Object::to_bool() const {
  bool rv;
    std::visit(overloaded {
      [] (void*) { throw std::bad_variant_access(); },
        [&rv] (auto&& v) { rv = (bool)v; },
        [] (StringPtr ptr) { throw std::bad_variant_access(); },
        [] (ListPtr ptr) { throw std::bad_variant_access(); },
        [] (MapPtr ptr) { throw std::bad_variant_access(); }
    }, dat);
    return rv;
}

inline
Int Object::to_int() const {
  Int rv;
    std::visit(overloaded {
      [] (void*) { throw std::bad_variant_access(); },
        [&rv] (auto&& v) { rv = (Int)v; },
        [] (StringPtr ptr) { throw std::bad_variant_access(); },
        [] (ListPtr ptr) { throw std::bad_variant_access(); },
        [] (MapPtr ptr) { throw std::bad_variant_access(); }
    }, dat);
    return rv;
}

inline
UInt Object::to_uint() const {
  UInt rv;
    std::visit(overloaded {
      [] (void*) { throw std::bad_variant_access(); },
        [&rv] (auto&& v) { rv = (UInt)v; },
        [] (StringPtr ptr) { throw std::bad_variant_access(); },
        [] (ListPtr ptr) { throw std::bad_variant_access(); },
        [] (MapPtr ptr) { throw std::bad_variant_access(); }
    }, dat);
    return rv;
}

inline
Float Object::to_fp() const {
  double rv;
    std::visit(overloaded {
      [] (void*) { throw std::bad_variant_access(); },
        [&rv] (auto&& v) { rv = (double)v; },
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
    [&ss] (auto&& v) { ss << v; },
    [&ss] (void*) { ss << "null"; },
    [&ss] (StringPtr p) { ss << std::get<0>(*p); },
    [this, &ss] (ListPtr p) { ss << to_json(); },
    [this, &ss] (MapPtr p) { ss << to_json(); },
  }, dat);
  return ss.str();
}

inline
Node& Object::operator [] (auto&& v) {
  if (is_list()) {
    return Node{std::get<0>(*std::get<ListPtr>(dat))[v]};
  } else if (is_map()) {
    return Node{std::get<0>(*std::get<MapPtr>(dat))[v]};
  }
  throw std::bad_variant_access();
}

inline
Node& Object::operator [] (const char* key) {
  if (is_map()) {
    return Node{std::get<0>(*std::get<MapPtr>(dat))[key]};
  }
  throw std::bad_variant_access();
}

inline
Node& Object::operator [] (const Key& key) {
  if (is_map()) {
    return Node{std::get<0>(*std::get<MapPtr>(dat))[key]};
  }
  throw std::bad_variant_access();
}

inline
Node& Object::operator [] (const Object& obj) {
  if (is_list()) {
    return Node{std::get<0>(*std::get<ListPtr>(dat))[obj.to_int()]};
  } else if (is_map()) {
    return Node{std::get<0>(*std::get<MapPtr>(dat))[obj.to_key()]};
  }
  throw std::bad_variant_access();
}

inline
Node& Object::operator [] (Key&& key) {
  if (is_map()) {
    return Node{std::get<0>(*std::get<MapPtr>(dat))[key]};
  }
  throw std::bad_variant_access();
}

inline
Node& Object::operator [] (Object&& obj) {
  if (is_list()) {
    return Node{std::get<0>(*std::get<ListPtr>(dat))[obj.to_int()]};
  } else if (is_map()) {
    return Node{std::get<0>(*std::get<MapPtr>(dat))[obj.to_key()]};
  }
  throw std::bad_variant_access();
}

inline
bool Object::operator == (const Object& obj) const {
  bool rv;
  std::visit(overloaded {
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
    [this, &obj, &rv] (auto&& lhs) {
      std::visit(overloaded {
        [] (void*) { throw std::bad_variant_access(); },
        [lhs, &rv] (auto&& rhs) { if (lhs < rhs) rv = -1; else if (lhs > rhs) rv = 1; else rv = 0; },
        [this, &rv] (StringPtr rhs) { rv = std::get<0>(*rhs).compare(as_str()); },
        [] (ListPtr) { throw std::bad_variant_access(); },
        [] (MapPtr) { throw std::bad_variant_access(); }
      }, obj.dat);
    },
    [&obj, &rv] (StringPtr lhs) { rv = std::get<0>(*lhs).compare(obj.as_str()); },
    [] (ListPtr p) { throw std::bad_variant_access(); },
    [] (MapPtr p) { throw std::bad_variant_access(); }
  }, dat);
  return rv;
}

inline
Key Object::to_key() const {
  Key key;
  std::visit(overloaded {
    [] (void* v) { throw std::bad_variant_access(); },
    [&key] (bool v) { key = (Int)v; },
        [&key] (Int v) { key = v; },
        [&key] (UInt v) { key = v; },
        [&key] (double v) { key = v; },
        [&key] (StringPtr ptr) { key = std::get<0>(*ptr); },
        [] (ListPtr ptr) { throw std::bad_variant_access(); },
        [] (MapPtr ptr) { throw std::bad_variant_access(); }
  }, dat);
  return key;
}

class WalkDF
{
  public:
    using Item = std::tuple<Object, Key, Object, bool>;
    using Stack = std::stack<Item>;
    using Visitor = std::function<void(const Object&, const Key&, const Object&, bool)>;

  public:
    WalkDF(Object root, Visitor visitor)
      : visitor{visitor}
    {
        stack.emplace(Object(), 0, root, false);
    }

    bool next() {
        bool has_next = !stack.empty();
        if (has_next) {
          const auto item = stack.top();
            stack.pop();

      std::visit(overloaded {
        [this, &item] (auto&& v) {
          auto& [parent, key, object, is_last] = item;
          visitor(parent, key, object, is_last);
        },
        [this, &item] (const ListPtr ptr) {
          auto& [parent, key, object, is_last] = item;
          visitor(parent, key, object, is_last);
          auto& list = std::get<0>(*ptr);
          Int index = list.size() - 1;
          auto it = list.crbegin();
          if (it != list.crend()) {
            stack.emplace(object, index, *it, true);
            it++; index--;
            for (; it != list.crend(); it++, index--) {
              stack.emplace(object, index, *it, false);
            }
          }
        },
        [this, &item] (const MapPtr ptr) {
          auto& [parent, key, object, is_last] = item;
          visitor(parent, key, object, is_last);
          auto& map = std::get<0>(*ptr);
          auto it = map.rcbegin();
          if (it != map.rcend()) {
            auto& [key, child] = *it;
            stack.emplace(object, key, child, true);
            it++;
            for (; it != map.rcend(); it++) {
              auto& [key, child] = *it;
              stack.emplace(object, key, child, false);
            };
          }
        }
      }, std::get<2>(item).dat);
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
    std::string ss;
    std::string opens;

    auto visitor = [&ss, &opens] (const Object& parent, const Key& key, const Object& object, bool is_last) -> void {
    auto visit_prim = overloaded {
      [] (auto&& v) {},
      [&ss] (void*) { ss += "null"; },
      [&ss] (bool v) { ss += v? "true": "false"; },
      [&ss] (Int v) { ss += std::to_string(v); },
      [&ss] (UInt v) { ss += std::to_string(v); },
      [&ss] (double v) { ss += std::to_string(v); },
      [&ss] (StringPtr p) { ss += nodel::quoted(std::get<0>(*p)); },
      [&ss, &opens] (ListPtr p) {
        ss += '[';
        opens.insert(opens.begin(), ']');
      },
      [&ss, &opens] (MapPtr p) {
        ss += '{';
        opens.insert(opens.begin(), '}');
      }
    };

    bool is_parent_container = true;

    if (parent.is_list()) {
      std::visit(visit_prim, object.dat);
    } else if (parent.is_map()) {
      ss += key_to_str(key);
      ss += ": ";
      std::visit(visit_prim, object.dat);
    } else {
      is_parent_container = false;
      std::visit(visit_prim, object.dat);
    }

    if (is_parent_container) {
      if (is_last) {
        ss += opens;
        opens.clear();
      } else {
        ss += ", ";
      }
    }
    };

    WalkDF walk{*this, visitor};
    while (walk.next());

    if (opens.size() > 0) ss += opens;

    return ss;
}

inline
Int Object::id() const {
  Int id = 0;
    std::visit(overloaded {
        [&id] (void* p) { id = 0; },
    [&id] (auto&& v) { id = (Int)v; },
        [&id] (const StringPtr p) { id = (Int)&(std::get<0>(*p)); },
        [&id] (const ListPtr p) { id = (Int)&(std::get<0>(*p)); },
        [&id] (const MapPtr p) { id = (Int)&(std::get<0>(*p)); }
    }, dat);
    return id;
}

inline
size_t Object::hash() const {
  Int id = 0;
    std::visit(overloaded {
        [&id] (void* p) { id = 0; },
    [&id] (auto&& v) { id = (size_t)v; },
        [&id] (const StringPtr p) { id = std::hash<std::string>{}(std::get<0>(*p)); },
    [] (ListPtr) { throw std::bad_variant_access(); },
    [] (MapPtr) { throw std::bad_variant_access(); }
    }, dat);
    return id;
}

inline
size_t Object::ref_count() const {
  size_t count = std::numeric_limits<size_t>::max();
    std::visit(overloaded {
    [] (auto&& v) {},
        [&count] (const StringPtr p) { count = std::get<1>(*p); },
        [&count] (const ListPtr p) { count = std::get<1>(*p); },
        [&count] (const MapPtr p) { count = std::get<1>(*p);  }
    }, dat);
    return count;
}


inline
Node& Node::operator [] (auto&& v) {
  if (is_list()) {
    return Node{this, std::get<0>(*std::get<ListPtr>(dat))[v]};
  } else if (is_map()) {
    return Node{std::get<0>(*std::get<MapPtr>(dat))[v]};
  }
  throw std::bad_variant_access();
}

inline
Node& Node::operator [] (const char* key) {
  if (is_map()) {
    return Node{std::get<0>(*std::get<MapPtr>(dat))[key]};
  }
  throw std::bad_variant_access();
}

inline
Node& Node::operator [] (const Key& key) {
  if (is_map()) {
    return Node{std::get<0>(*std::get<MapPtr>(dat))[key]};
  }
  throw std::bad_variant_access();
}

inline
Node& Node::operator [] (const Object& obj) {
  if (is_list()) {
    return Node{std::get<0>(*std::get<ListPtr>(dat))[obj.to_int()]};
  } else if (is_map()) {
    return Node{std::get<0>(*std::get<MapPtr>(dat))[obj.to_key()]};
  }
  throw std::bad_variant_access();
}

inline
Node& Node::operator [] (Key&& key) {
  if (is_map()) {
    return Node{std::get<0>(*std::get<MapPtr>(dat))[key]};
  }
  throw std::bad_variant_access();
}

inline
Node& Node::operator [] (Object&& obj) {
  if (is_list()) {
    return Node{std::get<0>(*std::get<ListPtr>(dat))[obj.to_int()]};
  } else if (is_map()) {
    return Node{std::get<0>(*std::get<MapPtr>(dat))[obj.to_key()]};
  }
  throw std::bad_variant_access();
}


}
// nodel namespace

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
