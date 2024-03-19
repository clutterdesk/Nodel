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

#include <memory>

class ValueIterator
{
  public:
    using iterator_category = std::forward_iterator_tag;
    using difference_type   = std::ptrdiff_t;
    using value_type        = const Object;
    using pointer           = const Object*;
    using reference         = const Object&;

  private:
    using DsIterPtr = std::unique_ptr<DataSource::ValueIterator>;
    using ReprType = Object::ReprType;

    union Repr
    {
        Repr()                  : pdi{nullptr} {}
        Repr(List::iterator it) : li{it} {}
        Repr(Map::iterator it)  : mi{it} {}
        Repr(DsIterPtr&& p_it)  : pdi{std::forward<DsIterPtr>(p_it)} {}
        ~Repr() {}

        List::iterator li;
        Map::iterator mi;
        DsIterPtr pdi;
    };

  public:
    ValueIterator()                  : m_repr_ix{ReprType::NULL_I}, m_repr{} {}
    ValueIterator(List::iterator it) : m_repr_ix{ReprType::LIST_I}, m_repr{it} {}
    ValueIterator(Map::iterator it)  : m_repr_ix{ReprType::OMAP_I}, m_repr{it} {}
    ValueIterator(DsIterPtr&& p_it)  : m_repr_ix{ReprType::DSRC_I}, m_repr{std::forward<DsIterPtr>(p_it)} { m_repr.pdi->next(); }
    ~ValueIterator();

    ValueIterator(const ValueIterator& other) = delete;
    ValueIterator(ValueIterator&& other);
    auto& operator = (const ValueIterator& other) = delete;
    auto& operator = (ValueIterator&& other);

    auto& operator ++ ();
    const Object& operator * () const;
    bool operator == (const ValueIterator&) const;

  private:
    ReprType m_repr_ix;
    Repr m_repr;
};


class ValueRange
{
  using ReprType = Object::ReprType;

  public:
    ValueRange(const Object& obj)
      : m_obj{(obj.m_fields.repr_ix == Object::DSRC_I && !obj.m_repr.ds->is_sparse())? obj.m_repr.ds->get_cached(obj): obj} {}

    ValueRange(const ValueRange&) = default;
    ValueRange(ValueRange&&) = default;
    ValueRange& operator = (const ValueRange&) = default;
    ValueRange& operator = (ValueRange&&) = default;

    ValueIterator begin();
    ValueIterator end();

  private:
    Object m_obj;
};


inline
ValueIterator::~ValueIterator() {
    if (m_repr_ix == ReprType::DSRC_I) std::destroy_at(&m_repr.pdi);
}

//inline
//ValueIterator::ValueIterator(const ValueIterator& other) : m_repr_ix{other.m_repr_ix} {
//    switch (m_repr_ix) {
//        case ReprType::NULL_I: break;
//        case ReprType::LIST_I: m_repr.li = other.m_repr.li; break;
//        case ReprType::OMAP_I: m_repr.mi = other.m_repr.mi; break;
//        case ReprType::DSRC_I: m_repr.pdi = other.m_repr.pdi; break;
//        default:               throw Object::wrong_type(m_repr_ix);
//    }
//}

inline
ValueIterator::ValueIterator(ValueIterator&& other) : m_repr_ix{other.m_repr_ix} {
    switch (m_repr_ix) {
        case ReprType::NULL_I: break;
        case ReprType::LIST_I: m_repr.li = other.m_repr.li; break;
        case ReprType::OMAP_I: m_repr.mi = other.m_repr.mi; break;
        case ReprType::DSRC_I: m_repr.pdi = std::move(other.m_repr.pdi); break;
        default:               throw Object::wrong_type(m_repr_ix);
    }
}

//inline
//auto& ValueIterator::operator = (const ValueIterator& other) {
//    m_repr_ix = other.m_repr_ix;
//    switch (m_repr_ix) {
//        case ReprType::NULL_I: break;
//        case ReprType::LIST_I: m_repr.li = other.m_repr.li; break;
//        case ReprType::OMAP_I: m_repr.mi = other.m_repr.mi; break;
//        case ReprType::DSRC_I: m_repr.pdi = other.m_repr.pdi; break;
//        default:               throw Object::wrong_type(m_repr_ix);
//    }
//    return *this;
//}

inline
auto& ValueIterator::operator = (ValueIterator&& other) {
    m_repr_ix = other.m_repr_ix;
    switch (m_repr_ix) {
        case ReprType::NULL_I: break;
        case ReprType::LIST_I: m_repr.li = other.m_repr.li; break;
        case ReprType::OMAP_I: m_repr.mi = other.m_repr.mi; break;
        case ReprType::DSRC_I: m_repr.pdi.release(); m_repr.pdi = std::move(other.m_repr.pdi); break;
        default:               throw Object::wrong_type(m_repr_ix);
    }
    return *this;
}

inline
auto& ValueIterator::operator ++ () {
    switch (m_repr_ix) {
        case ReprType::LIST_I: ++(m_repr.li); break;
        case ReprType::OMAP_I: ++(m_repr.mi); break;
        case ReprType::DSRC_I: m_repr.pdi->next(); break;
        default:               throw Object::wrong_type(m_repr_ix);
    }
    return *this;
}

inline
const Object& ValueIterator::operator * () const {
    switch (m_repr_ix) {
        case ReprType::LIST_I: return *m_repr.li;
        case ReprType::OMAP_I: return m_repr.mi->second;
        case ReprType::DSRC_I: return m_repr.pdi->value();
        default:               throw Object::wrong_type(m_repr_ix);
    }
}

inline
bool ValueIterator::operator == (const ValueIterator& other) const {
    switch (m_repr_ix) {
        case ReprType::NULL_I: return other.m_repr_ix == ReprType::NULL_I || other == *this;
        case ReprType::LIST_I: return m_repr.li == other.m_repr.li;
        case ReprType::OMAP_I: return m_repr.mi == other.m_repr.mi;
        case ReprType::DSRC_I: return m_repr.pdi->done() && other.m_repr_ix == ReprType::NULL_I;
        default:               throw Object::wrong_type(m_repr_ix);
    }
}

inline
ValueIterator ValueRange::begin() {
    auto repr_ix = m_obj.m_fields.repr_ix;
    switch (repr_ix) {
        case ReprType::LIST_I: return ValueIterator{std::get<0>(*m_obj.m_repr.pl).begin()};
        case ReprType::OMAP_I: return ValueIterator{std::get<0>(*m_obj.m_repr.pm).begin()};
        case ReprType::DSRC_I: {
            auto p_it = m_obj.m_repr.ds->value_iter();
            return (p_it)? ValueIterator{std::move(p_it)}: ValueIterator{};
        }
        default: throw Object::wrong_type(repr_ix);
    }
}

inline
ValueIterator ValueRange::end() {
    auto repr_ix = m_obj.m_fields.repr_ix;
    switch (repr_ix) {
        case ReprType::LIST_I: return ValueIterator{std::get<0>(*m_obj.m_repr.pl).end()};
        case ReprType::OMAP_I: return ValueIterator{std::get<0>(*m_obj.m_repr.pm).end()};
        case ReprType::DSRC_I: return ValueIterator{};
        default: throw Object::wrong_type(repr_ix);
    }
}
