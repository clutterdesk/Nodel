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

    using ReprType = Object::ReprType;

    union Repr
    {
        Repr()                              : pdi{nullptr} {}
        Repr(size_t pos, List::iterator it) : li{pos, it} {}
        Repr(Map::iterator it)              : smi{it} {}
        Repr(OMap::iterator it)             : omi{it} {}
        Repr(DsIterPtr&& p_it)              : pdi{std::forward<DsIterPtr>(p_it)} {}
        ~Repr() {}

        ListIterator li;
        Map::iterator smi;
        OMap::iterator omi;
        DsIterPtr pdi;
    };

  public:
    ItemIterator()                              : m_repr_ix{ReprType::NULL_I} {}
    ItemIterator(size_t pos, List::iterator it) : m_repr_ix{ReprType::LIST_I}, m_repr{pos, it} {}
    ItemIterator(Map::iterator it)              : m_repr_ix{ReprType::MAP_I}, m_repr{it} {}
    ItemIterator(OMap::iterator it)             : m_repr_ix{ReprType::OMAP_I}, m_repr{it} {}
    ItemIterator(DsIterPtr&& p_it)              : m_repr_ix{ReprType::DSRC_I}, m_repr{std::forward<DsIterPtr>(p_it)} { m_repr.pdi->next(); }
    ~ItemIterator();
    
    ItemIterator(const ItemIterator& other) = delete;
    ItemIterator(ItemIterator&& other);
    auto& operator = (const ItemIterator& other) = delete;
    auto& operator = (ItemIterator&& other);

    auto& operator ++ ();
    const Item& operator * ();
    bool operator == (const ItemIterator&) const;

  private:
    ReprType m_repr_ix;
    Repr m_repr;
    Item m_item;
};


class ItemRange
{
  using ReprType = Object::ReprType;

  public:
    ItemRange(const Object& obj, const Interval& itvl)
      : m_obj{(obj.m_fields.repr_ix == Object::DSRC_I && !obj.m_repr.ds->is_sparse())? obj.m_repr.ds->get_cached(obj): obj}
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
    if (m_repr_ix == ReprType::DSRC_I) std::destroy_at(&m_repr.pdi);
}

inline
ItemIterator::ItemIterator(ItemIterator&& other) : m_repr_ix{other.m_repr_ix} {
    switch (m_repr_ix) {
        case ReprType::NULL_I: break;
        case ReprType::LIST_I: m_repr.li = other.m_repr.li; break;
        case ReprType::MAP_I:  m_repr.smi = other.m_repr.smi; break;
        case ReprType::OMAP_I: m_repr.omi = other.m_repr.omi; break;
        case ReprType::DSRC_I: m_repr.pdi = std::move(other.m_repr.pdi); break;
        default:     throw Object::wrong_type(m_repr_ix);
    }
}

inline
auto& ItemIterator::operator = (ItemIterator&& other) {
    m_repr_ix = other.m_repr_ix;
    switch (m_repr_ix) {
        case ReprType::NULL_I: break;
        case ReprType::LIST_I: m_repr.li = other.m_repr.li; break;
        case ReprType::MAP_I:  m_repr.smi = other.m_repr.smi; break;
        case ReprType::OMAP_I: m_repr.omi = other.m_repr.omi; break;
        case ReprType::DSRC_I: m_repr.pdi = std::move(other.m_repr.pdi); break;
        default:     throw Object::wrong_type(m_repr_ix);
    }
    return *this;
}

inline
auto& ItemIterator::operator ++ () {
    switch (m_repr_ix) {
        case ReprType::LIST_I: ++(std::get<0>(m_repr.li)); ++(std::get<1>(m_repr.li)); break;
        case ReprType::MAP_I:  ++(m_repr.smi); break;
        case ReprType::OMAP_I: ++(m_repr.omi); break;
        case ReprType::DSRC_I: m_repr.pdi->next(); break;
        default:     throw Object::wrong_type(m_repr_ix);
    }
    return *this;
}

inline
const Item& ItemIterator::operator * () {
    switch (m_repr_ix) {
        case ReprType::LIST_I: {
            m_item.first = std::get<0>(m_repr.li);
            m_item.second = *(std::get<1>(m_repr.li));
            return m_item;
        }
        case ReprType::MAP_I: {
            m_item = *m_repr.smi;
            return m_item;
        }
        case ReprType::OMAP_I: return *m_repr.omi;
        case ReprType::DSRC_I: return m_repr.pdi->item();
        default:     throw Object::wrong_type(m_repr_ix);
    }
}

inline
bool ItemIterator::operator == (const ItemIterator& other) const {
    switch (m_repr_ix) {
        case ReprType::NULL_I: return other.m_repr_ix == ReprType::NULL_I || other == *this;
        case ReprType::LIST_I: return std::get<0>(m_repr.li) == std::get<0>(other.m_repr.li);
        case ReprType::MAP_I:  return m_repr.smi == other.m_repr.smi;
        case ReprType::OMAP_I: return m_repr.omi == other.m_repr.omi;
        case ReprType::DSRC_I: return m_repr.pdi->done();
        default:     throw Object::wrong_type(m_repr_ix);
    }
}

inline
ItemIterator ItemRange::begin() {
    auto repr_ix = m_obj.m_fields.repr_ix;
    switch (repr_ix) {
        case ReprType::LIST_I: {
            auto& list = std::get<0>(*m_obj.m_repr.pl);
            if (m_itvl.is_empty()) {
                return ItemIterator{0UL, list.begin()};
            } else {
                auto indices = m_itvl.to_indices(list.size());
                return ItemIterator{indices.first, list.begin() + indices.first};
            }
        }
        case ReprType::MAP_I: {
            if (!m_itvl.is_empty()) throw WrongType(Object::type_name(repr_ix));
            return ItemIterator{std::get<0>(*m_obj.m_repr.psm).begin()};
        }
        case ReprType::OMAP_I: {
            if (!m_itvl.is_empty()) throw WrongType(Object::type_name(repr_ix));
            return ItemIterator{std::get<0>(*m_obj.m_repr.pom).begin()};
        }
        case ReprType::DSRC_I: {
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
        case ReprType::LIST_I: {
            auto& list = std::get<0>(*m_obj.m_repr.pl);
            if (m_itvl.is_empty()) {
                return ItemIterator{list.size(), list.end()};
            } else {
                auto indices = m_itvl.to_indices(list.size());
                return ItemIterator{indices.second, list.begin() + indices.second};
            }
        }
        case ReprType::MAP_I: {
            if (!m_itvl.is_empty()) throw WrongType(Object::type_name(repr_ix));
            return ItemIterator{std::get<0>(*m_obj.m_repr.psm).end()};
        }
        case ReprType::OMAP_I: {
            if (!m_itvl.is_empty()) throw WrongType(Object::type_name(repr_ix));
            return ItemIterator{std::get<0>(*m_obj.m_repr.pom).end()};
        }
        case ReprType::DSRC_I: return ItemIterator{};
        default: throw Object::wrong_type(repr_ix);
    }
}
