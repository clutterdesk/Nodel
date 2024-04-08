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
//  public:
//    using iterator_category = std::forward_iterator_tag;
//    using difference_type   = std::ptrdiff_t;
//    using value_type        = const Object;
//    using pointer           = const Object*;
//    using reference         = const Object&;
//
  private:
    using DsIterPtr = std::unique_ptr<DataSource::ValueIterator>;
    using ReprIX = Object::ReprIX;

    union Repr
    {
        Repr()                  : pdi{nullptr} {}
        Repr(List::iterator it) : li{it} {}
        Repr(SortedMap::iterator it)  : smi{it} {}
        Repr(OrderedMap::iterator it) : omi{it} {}
        Repr(DsIterPtr&& p_it)  : pdi{std::forward<DsIterPtr>(p_it)} {}
        ~Repr() {}

        List::iterator li;
        SortedMap::iterator smi;
        OrderedMap::iterator omi;
        DsIterPtr pdi;
    };

  public:
    ValueIterator()                        : m_repr_ix{ReprIX::NIL}, m_repr{} {}
    ValueIterator(List::iterator it)       : m_repr_ix{ReprIX::LIST}, m_repr{it} {}
    ValueIterator(SortedMap::iterator it)  : m_repr_ix{ReprIX::MAP}, m_repr{it} {}
    ValueIterator(OrderedMap::iterator it) : m_repr_ix{ReprIX::OMAP}, m_repr{it} {}
    ValueIterator(DsIterPtr&& p_it)        : m_repr_ix{ReprIX::DSRC}, m_repr{std::forward<DsIterPtr>(p_it)} { m_repr.pdi->next(); }
    ~ValueIterator();

    ValueIterator(const ValueIterator& other) = delete;
    ValueIterator(ValueIterator&& other);
    auto& operator = (const ValueIterator& other) = delete;
    auto& operator = (ValueIterator&& other);

    auto& operator ++ ();
    const Object& operator * () const;
    bool operator == (const ValueIterator&) const;

  private:
    ReprIX m_repr_ix;
    Repr m_repr;
};


class ValueRange
{
  using ReprIX = Object::ReprIX;

  public:
    ValueRange() = default;

    ValueRange(const Object& obj, const Interval& itvl)
      : m_obj{(obj.m_fields.repr_ix == Object::DSRC && !obj.m_repr.ds->is_sparse())? obj.m_repr.ds->get_cached(obj): obj}
      , m_itvl{itvl}
    {}

    ValueRange(const Object& obj) : ValueRange(obj, {}) {}

    ValueIterator begin();
    ValueIterator end();

  private:
    Object m_obj;
    Interval m_itvl;
};


inline
ValueIterator::~ValueIterator() {
    if (m_repr_ix == ReprIX::DSRC) std::destroy_at(&m_repr.pdi);
}

inline
ValueIterator::ValueIterator(ValueIterator&& other) : m_repr_ix{other.m_repr_ix} {
    switch (m_repr_ix) {
        case ReprIX::NIL: break;
        case ReprIX::LIST: m_repr.li = other.m_repr.li; break;
        case ReprIX::MAP:  m_repr.smi = other.m_repr.smi; break;
        case ReprIX::OMAP: m_repr.omi = other.m_repr.omi; break;
        case ReprIX::DSRC: m_repr.pdi = std::move(other.m_repr.pdi); break;
        default:               throw Object::wrong_type(m_repr_ix);
    }
}

inline
auto& ValueIterator::operator = (ValueIterator&& other) {
    m_repr_ix = other.m_repr_ix;
    switch (m_repr_ix) {
        case ReprIX::NIL: break;
        case ReprIX::LIST: m_repr.li = other.m_repr.li; break;
        case ReprIX::MAP:  m_repr.smi = other.m_repr.smi; break;
        case ReprIX::OMAP: m_repr.omi = other.m_repr.omi; break;
        case ReprIX::DSRC: m_repr.pdi.release(); m_repr.pdi = std::move(other.m_repr.pdi); break;
        default:               throw Object::wrong_type(m_repr_ix);
    }
    return *this;
}

inline
auto& ValueIterator::operator ++ () {
    switch (m_repr_ix) {
        case ReprIX::LIST: ++(m_repr.li); break;
        case ReprIX::MAP:  ++(m_repr.smi); break;
        case ReprIX::OMAP: ++(m_repr.omi); break;
        case ReprIX::DSRC: m_repr.pdi->next(); break;
        default:               throw Object::wrong_type(m_repr_ix);
    }
    return *this;
}

inline
const Object& ValueIterator::operator * () const {
    switch (m_repr_ix) {
        case ReprIX::LIST: return *m_repr.li;
        case ReprIX::MAP:  return m_repr.smi->second;
        case ReprIX::OMAP: return m_repr.omi->second;
        case ReprIX::DSRC: return m_repr.pdi->value();
        default:               throw Object::wrong_type(m_repr_ix);
    }
}

inline
bool ValueIterator::operator == (const ValueIterator& other) const {
    switch (m_repr_ix) {
        case ReprIX::NIL: return other.m_repr_ix == ReprIX::NIL || other == *this;
        case ReprIX::LIST: return m_repr.li == other.m_repr.li;
        case ReprIX::MAP:  return m_repr.smi == other.m_repr.smi;
        case ReprIX::OMAP: return m_repr.omi == other.m_repr.omi;
        case ReprIX::DSRC: return m_repr.pdi->done() && other.m_repr_ix == ReprIX::NIL;
        default:               throw Object::wrong_type(m_repr_ix);
    }
}

inline
ValueIterator ValueRange::begin() {
    auto repr_ix = m_obj.m_fields.repr_ix;
    switch (repr_ix) {
        case ReprIX::LIST: {
            auto& list = std::get<0>(*m_obj.m_repr.pl);
            if (m_itvl.min().value() == nil) {
                return ValueIterator{list.begin()};
            } else {
                auto indices = m_itvl.to_indices(list.size());
                return ValueIterator{list.begin() + indices.first};
            }
        }
        case ReprIX::MAP: {
            auto& map = std::get<0>(*m_obj.m_repr.psm);
            auto& min_key = m_itvl.min().value();
            if (min_key == nil) {
                return ValueIterator{map.begin()};
            } else {
                auto it = map.lower_bound(min_key);
                if (m_itvl.min().is_open() && it != map.end())
                    ++it;
                return ValueIterator{it};
            }
        }
        case ReprIX::OMAP: {
            if (!m_itvl.is_empty()) throw WrongType(Object::type_name(repr_ix));
            return ValueIterator{std::get<0>(*m_obj.m_repr.pom).begin()};
        }
        case ReprIX::DSRC: {
            auto p_it = m_itvl.is_empty()? m_obj.m_repr.ds->value_iter(): m_obj.m_repr.ds->value_iter(m_itvl);
            return (p_it)? ValueIterator{std::move(p_it)}: ValueIterator{};
        }
        default: throw Object::wrong_type(repr_ix);
    }
}

inline
ValueIterator ValueRange::end() {
    auto repr_ix = m_obj.m_fields.repr_ix;
    switch (repr_ix) {
        case ReprIX::LIST: {
            auto& list = std::get<0>(*m_obj.m_repr.pl);
            if (m_itvl.max().value() == nil) {
                return ValueIterator{list.end()};
            } else {
                auto indices = m_itvl.to_indices(list.size());
                return ValueIterator{list.begin() + indices.second};
            }
        }
        case ReprIX::MAP: {
            auto& map = std::get<0>(*m_obj.m_repr.psm);
            auto& max_key = m_itvl.max().value();
            if (max_key == nil) {
                return ValueIterator{map.end()};
            } else {
                auto it = map.upper_bound(max_key);
                if (m_itvl.max().is_open())
                    --it;
                return ValueIterator{it};
            }
        }
        case ReprIX::OMAP: {
            if (!m_itvl.is_empty()) throw WrongType(Object::type_name(repr_ix));
            return ValueIterator{std::get<0>(*m_obj.m_repr.pom).end()};
        }
        case ReprIX::DSRC: return ValueIterator{};
        default: throw Object::wrong_type(repr_ix);
    }
}