/// @b License: @n Apache License v2.0
/// @copyright Robert Dunnagan
#pragma once

#include <rocksdb/db.h>
#include <rocksdb/comparator.h>
#include <nodel/core/Object.hxx>
#include <nodel/kvdb/serialize.hxx>

namespace nodel::kvdb {

class Comparator : public ::rocksdb::Comparator
{
  public:
    using Slice = ::rocksdb::Slice;

    const char* Name() const override { return "nodel.key.comparator.v1"; }

    int Compare(const Slice&, const Slice&) const override;

    // If *start < limit, changes *start to a short string in [start,limit).
    // Simple comparator implementations may return with *start unchanged,
    // i.e., an implementation of this method that does nothing is correct.
    void FindShortestSeparator(std::string* start, const Slice& limit) const override;

    // Changes *key to a short string >= *key.
    // Simple comparator implementations may return with *key unchanged,
    // i.e., an implementation of this method that does nothing is correct.
    void FindShortSuccessor(std::string* key) const override;

    // given two keys, determine if t is the successor of s
    // BUG: only return true if no other keys starting with `t` are ordered
    // before `t`. Otherwise, the auto_prefix_mode can omit entries within
    // iterator bounds that have same prefix as upper bound but different
    // prefix from seek key.
    bool IsSameLengthImmediateSuccessor(const Slice& /*s*/, const Slice& /*t*/) const override;
};


inline
int Comparator::Compare(const Slice& lhs, const Slice& rhs) const {
    Key l_key;
    Key r_key;
    deserialize(lhs.ToStringView(), l_key);
    deserialize(rhs.ToStringView(), r_key);
    auto result = l_key <=> r_key;
    if (result == std::partial_ordering::less) return -1;
    if (result == std::partial_ordering::greater) return 1;
    return 0;
}

// If *start < limit, changes *start to a short string in [start,limit).
// Simple comparator implementations may return with *start unchanged,
// i.e., an implementation of this method that does nothing is correct.
inline
void Comparator::FindShortestSeparator(std::string* start, const Slice& limit) const {
}

// Changes *key to a short string >= *key.
// Simple comparator implementations may return with *key unchanged,
// i.e., an implementation of this method that does nothing is correct.
inline
void Comparator::FindShortSuccessor(std::string* key) const {
}

// given two keys, determine if t is the successor of s
// BUG: only return true if no other keys starting with `t` are ordered
// before `t`. Otherwise, the auto_prefix_mode can omit entries within
// iterator bounds that have same prefix as upper bound but different
// prefix from seek key.
bool Comparator::IsSameLengthImmediateSuccessor(const Slice& s, const Slice& t) const {
    return false;
}

} // namespace nodel::kvdb
