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

#include <unordered_set>
#include <string_view>
#include <nodel/types.h>

// Since this library is header-only, the following macro must be invoked before any
// any other library facilities are used.
#define NODELNTERN_STATIC_INIT \
thread_local std::unordered_set<nodel::StringView> thread_interns;

extern thread_local std::unordered_set<nodel::StringView> thread_interns;

namespace nodel {

class intern_t
{
  public:
    explicit intern_t(const StringView& str) : m_str{str} {}
    intern_t()                               : m_str{""} {}

    bool operator == (intern_t other) const          { return m_str == other.m_str; }
    bool operator == (const String& other) const     { return other == m_str; }
    bool operator == (const StringView& other) const { return other == m_str; }
    bool operator == (const char* other) const       { return m_str == other; }

    const StringView& data() const { return m_str; }

  private:
    StringView m_str;
};

inline
intern_t intern_string_literal(const StringView& literal) {
    if (auto match = thread_interns.find(literal); match != thread_interns.end()) {
        return intern_t{*match};
    } else {
        thread_interns.insert(literal);
        return intern_t{literal};
    }
}

inline
intern_t intern_string(const StringView& str) {
    if (auto match = thread_interns.find(str); match != thread_interns.end()) {
        return intern_t{*match};
    } else {
        char* p_copy = new char[str.size()];
        str.copy(p_copy, str.size());
        StringView copy{p_copy, str.size()};
        thread_interns.insert(copy);
        return intern_t{copy};
    }
}

} // nodel::impl namespace
