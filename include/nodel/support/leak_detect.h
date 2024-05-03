#pragma once

#include <unordered_map>

namespace nodel::debug {

#define NODEL_INIT_LEAK_DETECT namespace nodel::debug { thread_local std::unordered_map<const Object*, int> ref_tracker; }
extern std::unordered_map<const Object*, int> ref_tracker;

struct Object
{
    Object()                    { ref_tracker[(Object*)this] = 1; }
    Object(const Object& other) { ref_tracker[(Object*)this] = 1; }
    Object(Object&& other)      { ref_tracker[(Object*)this] = 1; }
    ~Object()                   { ref_tracker.at((Object*)this)--; }
};

}  // namespace nodel::debug
