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

#include "Object.h"
#include <memory>

class KeyIterator
{
  private:
    using DsIterPtr = std::unique_ptr<DataSource::KeyIterator>;
    using ReprType = Object::ReprType;

    union Repr
    {
        Repr()                 : li{0UL} {}
        Repr(UInt it)          : li{it} {}
        Repr(Map::iterator it) : mi{it} {}
        Repr(DsIterPtr&& p_it) : pdi{std::forward<DsIterPtr>(p_it)} {}
        ~Repr() {}

        Key li;
        Map::iterator mi;
        DsIterPtr pdi;
    };

  public:
    KeyIterator()                 : m_repr_ix{ReprType::NULL_I}, m_repr{} {}
    KeyIterator(size_t pos)       : m_repr_ix{ReprType::LIST_I}, m_repr{pos} {}
    KeyIterator(Map::iterator it) : m_repr_ix{ReprType::OMAP_I}, m_repr{it} {}
    KeyIterator(DsIterPtr&& p_it) : m_repr_ix{ReprType::DSRC_I}, m_repr{std::forward<DsIterPtr>(p_it)} { m_repr.pdi->next(); }
    ~KeyIterator();

    KeyIterator(const KeyIterator& other) = delete;
    KeyIterator(KeyIterator&& other);
    auto& operator = (const KeyIterator& other) = delete;
    auto& operator = (KeyIterator&& other);

    auto& operator ++ ();
    const Key& operator * () const;
    bool operator == (const KeyIterator&) const;

  private:
    ReprType m_repr_ix;
    Repr m_repr;
};


class KeyRange
{
  using ReprType = Object::ReprType;

  public:
    KeyRange(const Object& obj)
      : m_obj{(obj.m_fields.repr_ix == Object::DSRC_I && !obj.m_repr.ds->is_sparse())? obj.m_repr.ds->get_cached(obj): obj} {}
    KeyRange(const KeyRange&) = default;
    KeyRange(KeyRange&&) = default;
    KeyRange& operator = (const KeyRange&) = default;
    KeyRange& operator = (KeyRange&&) = default;

    KeyIterator begin();
    KeyIterator end();

  private:
    Object m_obj;
};


inline
KeyIterator::~KeyIterator() {
    switch (m_repr_ix) {
        case ReprType::LIST_I: std::destroy_at(&m_repr.li); break;
        case ReprType::DSRC_I: std::destroy_at(&m_repr.pdi); break;
        default:               break;
    }
}

//inline
//KeyIterator::KeyIterator(const KeyIterator& other) : m_repr_ix{other.m_repr_ix} {
//    switch (m_repr_ix) {
//        case ReprType::NULL_I: break;
//        case ReprType::LIST_I: m_repr.li = other.m_repr.li; break;
//        case ReprType::OMAP_I: m_repr.mi = other.m_repr.mi; break;
//        case ReprType::DSRC_I: m_repr.pdi = other.m_repr.pdi; break;
//        default:               throw Object::wrong_type(m_repr_ix);
//    }
//}

inline
KeyIterator::KeyIterator(KeyIterator&& other) : m_repr_ix{other.m_repr_ix} {
    switch (m_repr_ix) {
        case ReprType::NULL_I: break;
        case ReprType::LIST_I: m_repr.li = other.m_repr.li; break;
        case ReprType::OMAP_I: m_repr.mi = other.m_repr.mi; break;
        case ReprType::DSRC_I: m_repr.pdi = std::move(other.m_repr.pdi); break;
        default:               throw Object::wrong_type(m_repr_ix);
    }
}

//inline
//auto& KeyIterator::operator = (const KeyIterator& other) {
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
auto& KeyIterator::operator = (KeyIterator&& other) {
    m_repr_ix = other.m_repr_ix;
    switch (m_repr_ix) {
        case ReprType::NULL_I: break;
        case ReprType::LIST_I: m_repr.li = other.m_repr.li; break;
        case ReprType::OMAP_I: m_repr.mi = other.m_repr.mi; break;
        case ReprType::DSRC_I: m_repr.pdi = std::move(other.m_repr.pdi); break;
        default:               throw Object::wrong_type(m_repr_ix);
    }
    return *this;
}

inline
auto& KeyIterator::operator ++ () {
    switch (m_repr_ix) {
        case ReprType::LIST_I: m_repr.li = m_repr.li.as<UInt>() + 1; break;
        case ReprType::OMAP_I: ++(m_repr.mi); break;
        case ReprType::DSRC_I: m_repr.pdi->next(); break;
        default:               throw Object::wrong_type(m_repr_ix);
    }
    return *this;
}

inline
const Key& KeyIterator::operator * () const {
    switch (m_repr_ix) {
        case ReprType::LIST_I: return m_repr.li;
        case ReprType::OMAP_I: return m_repr.mi->first;
        case ReprType::DSRC_I: return m_repr.pdi->key();
        default:               throw Object::wrong_type(m_repr_ix);
    }
}

inline
bool KeyIterator::operator == (const KeyIterator& other) const {
    switch (m_repr_ix) {
        case ReprType::NULL_I: return other.m_repr_ix == ReprType::NULL_I || other == *this;
        case ReprType::LIST_I: return m_repr.li == other.m_repr.li;
        case ReprType::OMAP_I: return m_repr.mi == other.m_repr.mi;
        case ReprType::DSRC_I: return m_repr.pdi->done() && other.m_repr_ix == ReprType::NULL_I;
        default:               throw Object::wrong_type(m_repr_ix);
    }
}

inline
KeyIterator KeyRange::begin() {
    auto repr_ix = m_obj.m_fields.repr_ix;
    switch (repr_ix) {
        case ReprType::LIST_I: return KeyIterator{0UL};
        case ReprType::OMAP_I: return KeyIterator{std::get<0>(*m_obj.m_repr.pm).begin()};
        case ReprType::DSRC_I: {
            auto p_it = m_obj.m_repr.ds->key_iter();
            return (p_it)? KeyIterator{std::move(p_it)}: KeyIterator{};
        }
        default: throw Object::wrong_type(repr_ix);
    }
}

inline
KeyIterator KeyRange::end() {
    auto repr_ix = m_obj.m_fields.repr_ix;
    switch (repr_ix) {
        case ReprType::LIST_I: return KeyIterator{std::get<0>(*m_obj.m_repr.pl).size()};
        case ReprType::OMAP_I: return KeyIterator{std::get<0>(*m_obj.m_repr.pm).end()};
        case ReprType::DSRC_I: return KeyIterator{};
        default:               throw Object::wrong_type(repr_ix);
    }
}
