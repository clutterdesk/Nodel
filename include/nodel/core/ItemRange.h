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

#include <tuple>

class ItemIterator
{
  private:
    using DsIterPtr = std::unique_ptr<DataSource::ItemIterator>;
    using ListIterator = std::tuple<size_t, List::iterator>;

    using ReprIX = Object::ReprIX;

    union Repr
    {
        Repr()                            : pdi{nullptr} {}
        Repr(UInt pos, List::iterator it) : li{pos, it} {}
        Repr(SortedMap::iterator it)      : smi{it} {}
        Repr(OrderedMap::iterator it)     : omi{it} {}
        Repr(DsIterPtr&& p_it)            : pdi{std::forward<DsIterPtr>(p_it)} {}
        ~Repr() {}

        ListIterator li;
        SortedMap::iterator smi;
        OrderedMap::iterator omi;
        DsIterPtr pdi;
    };

  public:
    ItemIterator()                            : m_repr_ix{ReprIX::NIL} {}
    ItemIterator(UInt pos, List::iterator it) : m_repr_ix{ReprIX::LIST}, m_repr{pos, it} {}
    ItemIterator(SortedMap::iterator it)      : m_repr_ix{ReprIX::MAP}, m_repr{it} {}
    ItemIterator(OrderedMap::iterator it)     : m_repr_ix{ReprIX::OMAP}, m_repr{it} {}
    ItemIterator(DsIterPtr&& p_it)            : m_repr_ix{ReprIX::DSRC}, m_repr{std::forward<DsIterPtr>(p_it)} { m_repr.pdi->next(); }
    ~ItemIterator();
    
    ItemIterator(const ItemIterator& other) = delete;
    ItemIterator(ItemIterator&& other);
    auto& operator = (const ItemIterator& other) = delete;
    auto& operator = (ItemIterator&& other);

    auto& operator ++ ();
    const Item& operator * ();
    bool operator == (const ItemIterator&) const;

  private:
    ReprIX m_repr_ix;
    Repr m_repr;
    Item m_item;
};


class ItemRange
{
  using ReprIX = Object::ReprIX;

  public:
    ItemRange() = default;

    ItemRange(const Object& obj, const Interval& itvl)
      : m_obj{(obj.m_fields.repr_ix == Object::DSRC && !obj.m_repr.ds->is_sparse())? obj.m_repr.ds->get_cached(obj): obj}
      , m_itvl{itvl} {}

    ItemRange(const Object& obj) : ItemRange(obj, {}) {}

    ItemRange(const ItemRange&) = default;
    ItemRange(ItemRange&&) = default;
    ItemRange& operator = (const ItemRange&) = default;
    ItemRange& operator = (ItemRange&&) = default;

    ItemIterator begin();
    ItemIterator end();

  private:
    Object m_obj;
    Interval m_itvl;
};


inline
ItemIterator::~ItemIterator() {
    if (m_repr_ix == ReprIX::DSRC) std::destroy_at(&m_repr.pdi);
}

inline
ItemIterator::ItemIterator(ItemIterator&& other) : m_repr_ix{other.m_repr_ix} {
    switch (m_repr_ix) {
        case ReprIX::NIL: break;
        case ReprIX::LIST: m_repr.li = other.m_repr.li; break;
        case ReprIX::MAP:  m_repr.smi = other.m_repr.smi; break;
        case ReprIX::OMAP: m_repr.omi = other.m_repr.omi; break;
        case ReprIX::DSRC: m_repr.pdi = std::move(other.m_repr.pdi); break;
        default:     throw Object::wrong_type(m_repr_ix);
    }
}

inline
auto& ItemIterator::operator = (ItemIterator&& other) {
    m_repr_ix = other.m_repr_ix;
    switch (m_repr_ix) {
        case ReprIX::NIL: break;
        case ReprIX::LIST: m_repr.li = other.m_repr.li; break;
        case ReprIX::MAP:  m_repr.smi = other.m_repr.smi; break;
        case ReprIX::OMAP: m_repr.omi = other.m_repr.omi; break;
        case ReprIX::DSRC: m_repr.pdi = std::move(other.m_repr.pdi); break;
        default:     throw Object::wrong_type(m_repr_ix);
    }
    return *this;
}

inline
auto& ItemIterator::operator ++ () {
    switch (m_repr_ix) {
        case ReprIX::LIST: ++(std::get<0>(m_repr.li)); ++(std::get<1>(m_repr.li)); break;
        case ReprIX::MAP:  ++(m_repr.smi); break;
        case ReprIX::OMAP: ++(m_repr.omi); break;
        case ReprIX::DSRC: m_repr.pdi->next(); break;
        default:     throw Object::wrong_type(m_repr_ix);
    }
    return *this;
}

inline
const Item& ItemIterator::operator * () {
    switch (m_repr_ix) {
        case ReprIX::LIST: {
            m_item.first = std::get<0>(m_repr.li);
            m_item.second = *(std::get<1>(m_repr.li));
            return m_item;
        }
        case ReprIX::MAP: {
            m_item = *m_repr.smi;
            return m_item;
        }
        case ReprIX::OMAP: return *m_repr.omi;
        case ReprIX::DSRC: return m_repr.pdi->item();
        default:     throw Object::wrong_type(m_repr_ix);
    }
}

inline
bool ItemIterator::operator == (const ItemIterator& other) const {
    switch (m_repr_ix) {
        case ReprIX::NIL: return other.m_repr_ix == ReprIX::NIL || other == *this;
        case ReprIX::LIST: return std::get<0>(m_repr.li) == std::get<0>(other.m_repr.li);
        case ReprIX::MAP:  return m_repr.smi == other.m_repr.smi;
        case ReprIX::OMAP: return m_repr.omi == other.m_repr.omi;
        case ReprIX::DSRC: return m_repr.pdi->done();
        default:     throw Object::wrong_type(m_repr_ix);
    }
}

inline
ItemIterator ItemRange::begin() {
    auto repr_ix = m_obj.m_fields.repr_ix;
    switch (repr_ix) {
        case ReprIX::LIST: {
            auto& list = std::get<0>(*m_obj.m_repr.pl);
            if (m_itvl.min().value() == nil) {
                return ItemIterator{0UL, list.begin()};
            } else {
                auto indices = m_itvl.to_indices(list.size());
                return ItemIterator{indices.first, list.begin() + indices.first};
            }
        }
        case ReprIX::MAP: {
            auto& map = std::get<0>(*m_obj.m_repr.psm);
            auto& min_key = m_itvl.min().value();
            if (min_key == nil) {
                return ItemIterator{map.begin()};
            } else {
                auto it = map.lower_bound(min_key);
                if (m_itvl.min().is_open() && it != map.end())
                    ++it;
                return ItemIterator{it};
            }
        }
        case ReprIX::OMAP: {
            if (!m_itvl.is_empty()) throw WrongType(Object::type_name(repr_ix));
            return ItemIterator{std::get<0>(*m_obj.m_repr.pom).begin()};
        }
        case ReprIX::DSRC: {
            auto p_it = m_itvl.is_empty()? m_obj.m_repr.ds->item_iter(): m_obj.m_repr.ds->item_iter(m_itvl);
            return (p_it)? ItemIterator{std::move(p_it)}: ItemIterator{};
        }
        default: throw Object::wrong_type(repr_ix);
    }
}

inline
ItemIterator ItemRange::end() {
    auto repr_ix = m_obj.m_fields.repr_ix;
    switch (repr_ix) {
        case ReprIX::LIST: {
            auto& list = std::get<0>(*m_obj.m_repr.pl);
            if (m_itvl.is_empty()) {
                return ItemIterator{list.size(), list.end()};
            } else {
                auto indices = m_itvl.to_indices(list.size());
                return ItemIterator{indices.second, list.begin() + indices.second};
            }
        }
        case ReprIX::MAP: {
            auto& map = std::get<0>(*m_obj.m_repr.psm);
            auto& max_key = m_itvl.max().value();
            if (max_key == nil) {
                return ItemIterator{map.end()};
            } else {
                auto it = map.upper_bound(max_key);
                if (m_itvl.max().is_open())
                    --it;
                return ItemIterator{it};
            }
        }
        case ReprIX::OMAP: {
            if (!m_itvl.is_empty()) throw WrongType(Object::type_name(repr_ix));
            return ItemIterator{std::get<0>(*m_obj.m_repr.pom).end()};
        }
        case ReprIX::DSRC: return ItemIterator{};
        default: throw Object::wrong_type(repr_ix);
    }
}
