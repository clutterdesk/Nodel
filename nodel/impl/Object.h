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

// Key is allowed by design to take more memory than Datum
using Key = std::variant<
	int64_t,
	uint64_t,
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
    void*,
    bool,
    int64_t,
	uint64_t,
    double,
    StringPtr,
    ListPtr,
    MapPtr
>;

// Auto casting between bool, integers and floating-point, but not string, list, or map.
// Trying to find sweet-spot between Python-like flexibility and C++ performance.
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
    bool is_signed_int() const { return std::holds_alternative<int64_t>(dat); };
    bool is_unsigned_int() const { return std::holds_alternative<uint64_t>(dat); };
    bool is_int() const { return is_signed_int() || is_unsigned_int(); }
    bool is_double() const { return std::holds_alternative<double>(dat); };
    bool is_string() const { return std::holds_alternative<StringPtr>(dat); };
    bool is_number() const { return is_int() || is_double(); }
    bool is_list() const { return std::holds_alternative<ListPtr>(dat); }
    bool is_map() const { return std::holds_alternative<MapPtr>(dat); }
    bool is_container() const { return is_list() || is_map(); }

    int64_t& as_signed_int() { return std::get<int64_t>(dat); }
    uint64_t& as_unsigned_int() { return std::get<uint64_t>(dat); }
    double& as_double() { return std::get<double>(dat); }
    std::string& as_string() { return std::get<0>(*std::get<StringPtr>(dat)); }
    List& as_list() { return std::get<0>(*std::get<ListPtr>(dat)); }
    Map& as_map() { return std::get<0>(*std::get<MapPtr>(dat)); }

    operator bool () const;
    operator int64_t () const;
    operator uint64_t () const;
    operator double () const;
    operator std::string () const;

    Object& operator [] (auto&&);
    Object& operator [] (const char*);
    Object& operator [] (const Key&);
    Object& operator [] (const Object&);
    Object& operator [] (Key&&);
    Object& operator [] (Object&&);

    int operator <=> (const Object&) const;

    Key to_key() const;
    std::string to_str() const;
    std::string to_json() const;

    size_t ref_count() const;
    int64_t id() const;

    friend class WalkDF;
    friend class WalkBF;

  private:
    void free() {
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

inline
Object::operator bool () const {
	bool result;
    std::visit(overloaded {
    	[] (void*) { throw std::bad_variant_access(); },
        [&result] (auto&& v) { result = (bool)v; },
        [] (StringPtr ptr) { throw std::bad_variant_access(); },
        [] (ListPtr ptr) { throw std::bad_variant_access(); },
        [] (MapPtr ptr) { throw std::bad_variant_access(); }
    }, dat);
    return result;
}

inline
Object::operator int64_t () const {
	int64_t result;
    std::visit(overloaded {
    	[] (void*) { throw std::bad_variant_access(); },
        [&result] (auto&& v) { result = (int64_t)v; },
        [] (StringPtr ptr) { throw std::bad_variant_access(); },
        [] (ListPtr ptr) { throw std::bad_variant_access(); },
        [] (MapPtr ptr) { throw std::bad_variant_access(); }
    }, dat);
    return result;
}

inline
Object::operator uint64_t () const {
	uint64_t result;
    std::visit(overloaded {
    	[] (void*) { throw std::bad_variant_access(); },
        [&result] (auto&& v) { result = (uint64_t)v; },
        [] (StringPtr ptr) { throw std::bad_variant_access(); },
        [] (ListPtr ptr) { throw std::bad_variant_access(); },
        [] (MapPtr ptr) { throw std::bad_variant_access(); }
    }, dat);
    return result;
}

inline
Object::operator double () const {
	double result;
    std::visit(overloaded {
    	[] (void*) { throw std::bad_variant_access(); },
        [&result] (auto&& v) { result = (double)v; },
        [] (StringPtr ptr) { throw std::bad_variant_access(); },
        [] (ListPtr ptr) { throw std::bad_variant_access(); },
        [] (MapPtr ptr) { throw std::bad_variant_access(); }
    }, dat);
    return result;
}

inline
Object::operator std::string () const {
	return std::get<0>(*std::get<StringPtr>(dat));
}

inline
Object& Object::operator [] (auto&& v) {
	if (is_list()) {
		return std::get<0>(*std::get<ListPtr>(dat))[v];
	} else if (is_map()) {
		return std::get<0>(*std::get<MapPtr>(dat))[v];
	}
	throw std::bad_variant_access();
}

inline
Object& Object::operator [] (const char* key) {
	if (is_map()) {
		return std::get<0>(*std::get<MapPtr>(dat))[key];
	}
	throw std::bad_variant_access();
}

inline
Object& Object::operator [] (const Key& key) {
	if (is_map()) {
		return std::get<0>(*std::get<MapPtr>(dat))[key];
	}
	throw std::bad_variant_access();
}

inline
Object& Object::operator [] (const Object& obj) {
	if (is_list()) {
		return std::get<0>(*std::get<ListPtr>(dat))[(uint64_t)obj];
	} else if (is_map()) {
		return std::get<0>(*std::get<MapPtr>(dat))[obj.to_key()];
	}
	throw std::bad_variant_access();
}

inline
Object& Object::operator [] (Key&& key) {
	if (is_map()) {
		return std::get<0>(*std::get<MapPtr>(dat))[key];
	}
	throw std::bad_variant_access();
}

inline
Object& Object::operator [] (Object&& obj) {
	if (is_list()) {
		return std::get<0>(*std::get<ListPtr>(dat))[(uint64_t)obj];
	} else if (is_map()) {
		return std::get<0>(*std::get<MapPtr>(dat))[obj.to_key()];
	}
	throw std::bad_variant_access();
}

inline
int Object::operator <=> (const Object& obj) const {
	int result;
	std::visit(overloaded {
		[] (void *) { throw std::bad_variant_access(); },
		[&obj, &result] (auto&& v) {
			auto cmp = v <=> (decltype(v))obj;
			if (cmp < 0) result = -1; else result = (cmp > 0)? 1: 0;
		},
		[&obj, &result] (StringPtr p) {
			result = std::get<0>(*p).compare(obj);
		},
		[] (ListPtr p) { throw std::bad_variant_access(); },
		[] (MapPtr p) { throw std::bad_variant_access(); }
	}, dat);
	return result;
}

inline
Key Object::to_key() const {
	Key key;
	std::visit(overloaded {
		[] (void* v) { throw std::bad_variant_access(); },
		[&key] (bool v) { key = (int64_t)v; },
        [&key] (int64_t v) { key = v; },
        [&key] (uint64_t v) { key = v; },
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
					int64_t index = list.size() - 1;
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
					int64_t index = 0;
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
std::string Object::to_str() const {
	std::stringstream ss;
	std::visit(overloaded {
		[] (auto&& v) {
			std::stringstream ss;
			ss << v;
			return ss.str();
		},
		[] (void*) { return std::string{"null"}; },
		[] (StringPtr p) { return std::get<0>(*p); },
		[this] (ListPtr p) { return to_json(); },
		[this] (MapPtr p) { return to_json(); },
	}, dat);
	throw std::bad_variant_access();
}

inline
std::string Object::to_json() const {
    std::string ss;
    std::string opens;

    auto visitor = [&ss, &opens] (const Object& parent, const Key& key, const Object& object, bool is_last) -> void {
		auto visit_prim = overloaded {
			[] (auto&& v) {},
			[&ss] (void*) { ss += "null"; },
			[&ss] (bool v) { ss += v? "true": "false"; },
			[&ss] (int64_t v) { ss += std::to_string(v); },
			[&ss] (uint64_t v) { ss += std::to_string(v); },
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
int64_t Object::id() const {
	int64_t id = 0;
    std::visit(overloaded {
        [&id] (void* p) { id = 0; },
		[&id] (auto&& v) { id = (int64_t)v; },
        [&id] (const StringPtr p) { id = (int64_t)&(std::get<0>(*p)); },
        [&id] (const ListPtr p) { id = (int64_t)&(std::get<0>(*p)); },
        [&id] (const MapPtr p) { id = (int64_t)&(std::get<0>(*p)); }
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

} // nodel namespace
