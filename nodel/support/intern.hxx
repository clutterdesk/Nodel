/// @b License: @n Apache License v2.0
/// @copyright Robert Dunnagan
#pragma once

#include <unordered_set>
#include <nodel/support/types.hxx>

namespace nodel {

namespace impl {

/////////////////////////////////////////////////////////////////////////////
/// Returns the *thread-local* set of interned strings.
/////////////////////////////////////////////////////////////////////////////
inline
std::unordered_set<StringView>& thread_interns() {
    static thread_local std::unordered_set<StringView> interns;
    return interns;
}

} // namespace impl

class intern_t
{
  public:
    explicit intern_t(const StringView& str) : m_str{str} {}

    /// Interned strings are equal if and only if they share the same storage.
    bool operator == (intern_t other) const {
        return m_str.data() == other.m_str.data() && m_str.size() == other.m_str.size();
    }
    bool operator == (const String& other) const     { return m_str == other; }
    bool operator == (const StringView& other) const { return m_str == other; }
    bool operator == (const char* other) const       { return m_str == other; }

    const StringView& data() const { return m_str; }

  private:
    StringView m_str;
};

inline
intern_t intern_string_literal(const StringView& literal) {
    auto& thread_interns = impl::thread_interns();
    if (auto match = thread_interns.find(literal); match != thread_interns.end()) {
        return intern_t{*match};
    } else {
        thread_interns.insert(literal);
        return intern_t{literal};
    }
}

inline
intern_t intern_string(const StringView& str) {
    auto& thread_interns = impl::thread_interns();
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
