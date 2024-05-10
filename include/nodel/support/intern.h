/// @b License: @n Apache License v2.0
/// @copyright Robert Dunnagan
#pragma once

#include <unordered_set>
#include <string_view>
#include <nodel/support/types.h>

namespace nodel {

#define NODEL_INIT_INTERNS namespace nodel { thread_local std::unordered_set<nodel::StringView> thread_interns; }
extern thread_local std::unordered_set<nodel::StringView> thread_interns;

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
