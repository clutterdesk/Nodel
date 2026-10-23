/// @b License: @n Apache License v2.0
/// @copyright Robert Dunnagan
#pragma once

#include <atomic>
#include <cstdint>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <unordered_set>
#include <nodel/support/types.hxx>

namespace nodel {

namespace impl {

/////////////////////////////////////////////////////////////////////////////
/// Header stored immediately before the characters of an interned string.
/////////////////////////////////////////////////////////////////////////////
struct InternHeader
{
    uint32_t hash;
    uint32_t size;

    /// Returns the header of the interned string whose characters start at @p p.
    static InternHeader of(const char* p) {
        InternHeader header;
        std::memcpy(&header, p - sizeof(header), sizeof(header));
        return header;
    }
};

/////////////////////////////////////////////////////////////////////////////
/// A string to find in an @ref InternTable, with its precomputed hash.
/////////////////////////////////////////////////////////////////////////////
struct InternQuery
{
    StringView str;
    uint32_t hash;
};

/// Hashes an interned string by reading the hash from its header, so that
/// a string is only hashed once, when it is first looked up.
struct InternHash
{
    using is_transparent = void;
    size_t operator () (const char* p) const        { return InternHeader::of(p).hash; }
    size_t operator () (const InternQuery& q) const { return q.hash; }
};

struct InternEqual
{
    using is_transparent = void;
    static StringView view(const char* p) { return {p, InternHeader::of(p).size}; }
    bool operator () (const char* a, const char* b) const        { return a == b; }
    bool operator () (const char* p, const InternQuery& q) const { return view(p) == q.str; }
    bool operator () (const InternQuery& q, const char* p) const { return view(p) == q.str; }
};

/////////////////////////////////////////////////////////////////////////////
/// A *thread-local* set of interned strings, and a process-wide unique
/// identifier for the set.
/// - Each interned string is preceded by an @ref InternHeader.
/// - Interned strings are allocated in chunks, and are never freed, so they
///   remain valid after the thread exits.
/////////////////////////////////////////////////////////////////////////////
struct InternTable
{
    InternTable() : id{next_id()} {}

    static uint32_t next_id() {
        static std::atomic<uint32_t> next{0};
        return next.fetch_add(1, std::memory_order_relaxed);
    }

    /// Returns the characters of the interned copy of @p str.
    const char* intern(const StringView& str) {
        InternQuery query{str, (uint32_t)std::hash<StringView>{}(str)};
        if (auto match = strings.find(query); match != strings.end())
            return *match;
        return *strings.insert(copy(query)).first;
    }

    std::unordered_set<const char*, InternHash, InternEqual> strings;
    uint32_t id;

  private:
    const char* copy(const InternQuery& query) {
        auto& str = query.str;
        if (str.size() > std::numeric_limits<uint32_t>::max())
            throw std::length_error("interned string too long");
        InternHeader header{query.hash, (uint32_t)str.size()};
        char* p_block = allocate(sizeof(header) + str.size());
        std::memcpy(p_block, &header, sizeof(header));
        str.copy(p_block + sizeof(header), str.size());
        return p_block + sizeof(header);
    }

    char* allocate(size_t size) {
        constexpr size_t chunk_size = 4096;
        size = (size + alignof(InternHeader) - 1) & ~(alignof(InternHeader) - 1);
        if (size > chunk_size / 8) return new char[size];
        if ((size_t)(m_chunk_end - m_chunk_next) < size) {
            m_chunk_next = new char[chunk_size];
            m_chunk_end = m_chunk_next + chunk_size;
        }
        char* p = m_chunk_next;
        m_chunk_next += size;
        return p;
    }

    char* m_chunk_next = nullptr;
    char* m_chunk_end = nullptr;
};

/////////////////////////////////////////////////////////////////////////////
/// Returns the *thread-local* set of interned strings.
/////////////////////////////////////////////////////////////////////////////
inline
InternTable& thread_interns() {
    static thread_local InternTable interns;
    return interns;
}

} // namespace impl

/////////////////////////////////////////////////////////////////////////////
/// A string interned in a *thread-local* table.
/// - Strings interned by the same thread are equal if and only if they share
///   the same storage.
/// - Strings interned by different threads are compared by content.
/// - The hash is computed from the content, so it is the same on every thread.
/////////////////////////////////////////////////////////////////////////////
class intern_t
{
  public:
    bool operator == (intern_t other) const {
        if (m_p == other.m_p) return true;
        if (m_table == other.m_table || m_hash != other.m_hash) return false;
        return equal_content(other);
    }

    StringView data() const { return {m_p, header().size}; }
    uint32_t hash() const   { return m_hash; }

  private:
    /// @param p Characters of a string returned by @ref impl::InternTable::intern.
    /// @param table Identifier of the intern table that holds @p p.
    intern_t(const char* p, uint32_t table) : m_p{p}, m_hash{header().hash}, m_table{table} {}

    /// Compares strings interned by different threads.
    [[gnu::noinline]] bool equal_content(intern_t other) const { return data() == other.data(); }

    impl::InternHeader header() const { return impl::InternHeader::of(m_p); }

    const char* m_p;
    uint32_t m_hash;
    uint32_t m_table;

  friend intern_t intern_string(const StringView& str);
};

inline
intern_t intern_string(const StringView& str) {
    auto& interns = impl::thread_interns();
    return intern_t{interns.intern(str), interns.id};
}

/// Literals are copied like other strings, since interned storage must be
/// preceded by an @ref impl::InternHeader.
inline
intern_t intern_string_literal(const StringView& literal) {
    return intern_string(literal);
}

} // nodel::impl namespace
