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

#include <functional>

class OPath
{
  public:
    enum Axis {
        ROOT,
        ANCESTOR,
        PARENT,
        SELF,
        CHILD,
        SIBLING,
        DESCENDANT
    }

    using Predicate = bool(const Object&);

    class Step
    {
      public:
        Step(Axis axis) : axis{axis}, pred{nullptr} {}
        Step(Axis axis, const Key& key) : axis{axis}, key{key}, pred{nullptr} {}
        Step(Axis axis, Predicate pred) : axis{axis}, pred{pred} {}
        Step(Axis axis, const Key& key, Predicate pred) : axis{axis}, key{key}, pred{pred} {}

        Object eval(const Object&);

      private:
        Axis axis;
        Key key;
        Predicate pred;
    };

  public:
    OPath() {}
    OPath(const char* spec, char delimiter='.');

    OPath(const OPath&) = default;
    OPath(OPath&&) = default;
    OPath& operator = (const OPath&) = default;
    OPath& operator = (OPath&&) = default;

    Object lookup(Object obj) const {
        for (const auto& key : key_list) {
            obj.set_reference(obj.get(key));
        }
        return obj;
    }

    OPath operator += (const OPath& path) {
        for (auto& key : path.key_list) key_list.push_back(key);
        return *this;
    }

    OPath operator + (const OPath& path) const {
        OPath result;
        KeyList& kl = result.key_list;
        kl.reserve(key_list.size() + path.key_list.size());
        for (auto& key : key_list) kl.push_back(key);
        for (auto& key : path.key_list) kl.push_back(key);
        return result;
    }

    KeyList keys() const { return key_list; }

    std::string to_str(char delimiter='.') const {
        std::stringstream ss;
        auto it = key_list.cbegin();
        auto end = key_list.cend();
        if (it == end) {
            ss << delimiter;
        } else if (it != end) {
            bool is_int = it->is_any_int();
            if (is_int) ss << '[';
            for (auto c : it->to_str()) {
                if (c == delimiter) ss << '\\' << c; else ss << c;
            }
            if (is_int) ss << ']';
            while (++it != end) {
                is_int = it->is_any_int();
                ss << (is_int? '[': delimiter);
                for (auto c : it->to_str()) {
                    if (c == delimiter) ss << '\\' << c; else ss << c;
                }
                if (is_int) ss << ']';
            }
        }
        return ss.str();
    }

  private:
    void parse(const char* spec);
    void parse_step(const char*& it);

  private:
    KeyList key_list;

    friend class Object;
    friend class PathIterator;
};

class PathParser
{
  public:
    PathParser(const char* spec, char delimiter='.') : spec{spec}, delimiter{delimiter}, it{spec} {}

    Key parse_step();

  private:
    const char* spec;
    char delimiter;
    const char* it;
};


inline
Object OPath::Step::eval(const Object& obj) {
    switch (axis) {
        case ROOT: {
            assert(key.is_null());  // impossible
            Object root = obj.root();
            if (pred == nullptr || pred(root)) return root;
            break;
        }
        case ANCESTOR: {

            break;
        }
        case PARENT: { break; }
        case SELF: { break; }
        case CHILD: { break; }
        case SIBLING: { break; }
        case DESCENDANT:
    }
}

class PathIterator
{
  public:
    PathIterator(Object object, const OPath& path) : object{object}, path{path} {

    }
    PathIterator& operator ++ () {
        return *this;
    }
    Object operator * () const { return object; }
    bool operator == (const AncestorIterator& other) const {
        if (object.is_empty()) return other.object.is_empty();
        return !other.object.is_empty() && object.id() == other.object.id();
    }
  private:
    Object object;
    const OPath& path;
};

class PathRange
{
  public:
    PathRange(Object object, const OPath& path) : object{object.parent()}, path{path} {}
    PathIterator begin() const { return {object, path}; }
    PathIterator end() const   { return {Object{}, path}; }
  private:
    Object object;
    const OPath& path;
};
