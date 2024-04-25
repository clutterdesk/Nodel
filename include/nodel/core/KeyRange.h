#pragma once

#include <memory>

class KeyIterator
{
  private:
    using DsIterPtr = std::unique_ptr<DataSource::KeyIterator>;
    using ReprIX = Object::ReprIX;

    union Repr
    {
        Repr()                        : li{0} {}
        Repr(Int it)                  : li{it} {}
        Repr(SortedMap::iterator it)  : smi{it} {}
        Repr(OrderedMap::iterator it) : omi{it} {}
        Repr(DsIterPtr&& p_it)        : pdi{std::forward<DsIterPtr>(p_it)} {}
        ~Repr() {}

        Key li;
        SortedMap::iterator smi;
        OrderedMap::iterator omi;
        DsIterPtr pdi;
    };

  public:
    KeyIterator()                        : m_repr_ix{ReprIX::NIL}, m_repr{} {}
    KeyIterator(Int pos)                 : m_repr_ix{ReprIX::LIST}, m_repr{pos} {}
    KeyIterator(SortedMap::iterator it)  : m_repr_ix{ReprIX::SMAP}, m_repr{it} {}
    KeyIterator(OrderedMap::iterator it) : m_repr_ix{ReprIX::OMAP}, m_repr{it} {}
    KeyIterator(DsIterPtr&& p_it)        : m_repr_ix{ReprIX::DSRC}, m_repr{std::forward<DsIterPtr>(p_it)} { m_repr.pdi->next(); }
    ~KeyIterator();

    KeyIterator(const KeyIterator& other) = delete;
    KeyIterator(KeyIterator&& other);
    auto& operator = (const KeyIterator& other) = delete;
    auto& operator = (KeyIterator&& other);

    auto& operator ++ ();
    const Key& operator * () const;
    bool operator == (const KeyIterator&) const;

  private:
    ReprIX m_repr_ix;
    Repr m_repr;
};


class KeyRange
{
  using ReprIX = Object::ReprIX;

  public:
    KeyRange() = default;

    KeyRange(const Object& obj, const Slice& slice)
      : m_obj{(obj.m_fields.repr_ix == Object::DSRC && !obj.m_repr.ds->is_sparse())? obj.m_repr.ds->get_cached(obj): obj}
      , m_slice{slice}
    {}

    KeyRange(const Object& obj) : KeyRange(obj, {}) {}

    KeyRange(const KeyRange&) = default;
    KeyRange(KeyRange&&) = default;
    KeyRange& operator = (const KeyRange&) = default;
    KeyRange& operator = (KeyRange&&) = default;

    KeyIterator begin();
    KeyIterator end();

  private:
    Object m_obj;
    Slice m_slice;
};


inline
KeyIterator::~KeyIterator() {
    switch (m_repr_ix) {
        case ReprIX::LIST: std::destroy_at(&m_repr.li); break;
        case ReprIX::DSRC: std::destroy_at(&m_repr.pdi); break;
        default:           break;
    }
}

inline
KeyIterator::KeyIterator(KeyIterator&& other) : m_repr_ix{other.m_repr_ix} {
    switch (m_repr_ix) {
        case ReprIX::NIL:  break;
        case ReprIX::LIST: m_repr.li = other.m_repr.li; break;
        case ReprIX::SMAP:  m_repr.smi = other.m_repr.smi; break;
        case ReprIX::OMAP: m_repr.omi = other.m_repr.omi; break;
        case ReprIX::DSRC: m_repr.pdi = std::move(other.m_repr.pdi); break;
        default:           throw Object::wrong_type(m_repr_ix);
    }
}

inline
auto& KeyIterator::operator = (KeyIterator&& other) {
    m_repr_ix = other.m_repr_ix;
    switch (m_repr_ix) {
        case ReprIX::NIL:  break;
        case ReprIX::LIST: m_repr.li = other.m_repr.li; break;
        case ReprIX::SMAP:  m_repr.smi = other.m_repr.smi; break;
        case ReprIX::OMAP: m_repr.omi = other.m_repr.omi; break;
        case ReprIX::DSRC: m_repr.pdi = std::move(other.m_repr.pdi); break;
        default:           throw Object::wrong_type(m_repr_ix);
    }
    return *this;
}

inline
auto& KeyIterator::operator ++ () {
    switch (m_repr_ix) {
        case ReprIX::LIST: m_repr.li = m_repr.li.as<UInt>() + 1; break;
        case ReprIX::SMAP:  ++(m_repr.smi); break;
        case ReprIX::OMAP: ++(m_repr.omi); break;
        case ReprIX::DSRC: m_repr.pdi->next(); break;
        default:           throw Object::wrong_type(m_repr_ix);
    }
    return *this;
}

inline
const Key& KeyIterator::operator * () const {
    switch (m_repr_ix) {
        case ReprIX::LIST: return m_repr.li;
        case ReprIX::SMAP:  return m_repr.smi->first;
        case ReprIX::OMAP: return m_repr.omi->first;
        case ReprIX::DSRC: return m_repr.pdi->key();
        default:           throw Object::wrong_type(m_repr_ix);
    }
}

inline
bool KeyIterator::operator == (const KeyIterator& other) const {
    switch (m_repr_ix) {
        case ReprIX::NIL: return other.m_repr_ix == ReprIX::NIL || other == *this;
        case ReprIX::LIST: return m_repr.li == other.m_repr.li;
        case ReprIX::SMAP:  return m_repr.smi == other.m_repr.smi;
        case ReprIX::OMAP: return m_repr.omi == other.m_repr.omi;
        case ReprIX::DSRC: return m_repr.pdi->done() && other.m_repr_ix == ReprIX::NIL;
        default:           throw Object::wrong_type(m_repr_ix);
    }
}

inline
KeyIterator KeyRange::begin() {
    auto repr_ix = m_obj.m_fields.repr_ix;
    switch (repr_ix) {
        case ReprIX::LIST: {
            auto& list = std::get<0>(*m_obj.m_repr.pl);
            if (m_slice.min().value() == nil) {
                return KeyIterator{0};
            } else {
                auto indices = m_slice.to_indices(list.size());
                auto start = std::get<0>(indices);
                return KeyIterator{start};
            }
        }
        case ReprIX::SMAP: {
            auto& map = std::get<0>(*m_obj.m_repr.psm);
            auto& min_key = m_slice.min().value();
            if (min_key == nil) {
                return KeyIterator{map.begin()};
            } else {
                auto it = map.lower_bound(min_key);
                if (m_slice.min().is_open() && it != map.end())
                    ++it;
                return KeyIterator{it};
            }
        }
        case ReprIX::OMAP: {
            if (!m_slice.is_empty()) throw WrongType(Object::type_name(repr_ix));
            return KeyIterator{std::get<0>(*m_obj.m_repr.pom).begin()};
        }
        case ReprIX::DSRC: {
            auto p_it = m_slice.is_empty()? m_obj.m_repr.ds->key_iter(): m_obj.m_repr.ds->key_iter(m_slice);
            return (p_it)? KeyIterator{std::move(p_it)}: KeyIterator{};
        }
        default: throw Object::wrong_type(repr_ix);
    }
}

inline
KeyIterator KeyRange::end() {
    auto repr_ix = m_obj.m_fields.repr_ix;
    switch (repr_ix) {
        case ReprIX::LIST: {
            auto& list = std::get<0>(*m_obj.m_repr.pl);
            if (m_slice.max().value() == nil) {
                return KeyIterator{(Int)list.size()};
            } else {
                auto indices = m_slice.to_indices(list.size());
                auto stop = std::get<1>(indices);
                return KeyIterator{stop};
            }
        }
        case ReprIX::SMAP: {
            auto& map = std::get<0>(*m_obj.m_repr.psm);
            auto& max_key = m_slice.max().value();
            if (max_key == nil) {
                return KeyIterator{map.end()};
            } else {
                auto it = map.upper_bound(max_key);
                if (m_slice.max().is_open())
                    --it;
                return KeyIterator{it};
            }
        }
        case ReprIX::OMAP: {
            if (!m_slice.is_empty()) throw WrongType(Object::type_name(repr_ix));
            return KeyIterator{std::get<0>(*m_obj.m_repr.pom).end()};
        }
        case ReprIX::DSRC: return KeyIterator{};
        default: throw Object::wrong_type(repr_ix);
    }
}
