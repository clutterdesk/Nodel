class ItemIterator
{
  private:
    using DsIterPtr = std::unique_ptr<DataSource::ItemIterator>;
    using ListIterator = std::tuple<size_t, ObjectList::iterator>;
    using ReprIX = Object::ReprIX;

    union Repr
    {
        Repr()                                 : pdi{nullptr} {}
        Repr(Int pos, ObjectList::iterator it) : li{pos, it} {}
        Repr(SortedMap::iterator it)           : smi{it} {}
        Repr(OrderedMap::iterator it)          : omi{it} {}
        Repr(DsIterPtr&& p_it)                 : pdi{std::forward<DsIterPtr>(p_it)} {}
        ~Repr() {}

        ListIterator li;
        SortedMap::iterator smi;
        OrderedMap::iterator omi;
        DsIterPtr pdi;
    };

  public:
    ItemIterator()                                 : m_repr_ix{ReprIX::NIL} {}
    ItemIterator(Int pos, ObjectList::iterator it) : m_repr_ix{ReprIX::LIST}, m_repr{pos, it} {}
    ItemIterator(SortedMap::iterator it)           : m_repr_ix{ReprIX::SMAP}, m_repr{it} {}
    ItemIterator(OrderedMap::iterator it)          : m_repr_ix{ReprIX::OMAP}, m_repr{it} {}
    ItemIterator(DsIterPtr&& p_it)                 : m_repr_ix{ReprIX::DSRC}, m_repr{std::forward<DsIterPtr>(p_it)} {}
    ~ItemIterator();
    
    ItemIterator(ItemIterator& other);
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

    ItemRange(const Object& obj, const Slice& slice)
      : m_obj{(obj.m_fields.repr_ix == Object::DSRC && !obj.m_repr.ds->is_sparse())? obj.m_repr.ds->get_cached(obj): obj}
      , m_slice{slice} {}

    ItemRange(const Object& obj) : ItemRange(obj, {}) {}

    ItemRange(const ItemRange&) = default;
    ItemRange(ItemRange&&) = default;
    ItemRange& operator = (const ItemRange&) = default;
    ItemRange& operator = (ItemRange&&) = default;

    ItemIterator begin();
    ItemIterator end();

  private:
    Object m_obj;
    Slice m_slice;
};


inline
ItemIterator::~ItemIterator() {
    if (m_repr_ix == ReprIX::DSRC) std::destroy_at(&m_repr.pdi);
}

inline
ItemIterator::ItemIterator(ItemIterator& other) : m_repr_ix{other.m_repr_ix} {
    switch (m_repr_ix) {
        case ReprIX::NIL: break;
        case ReprIX::LIST: m_repr.li = other.m_repr.li; break;
        case ReprIX::SMAP:  m_repr.smi = other.m_repr.smi; break;
        case ReprIX::OMAP: m_repr.omi = other.m_repr.omi; break;
        case ReprIX::DSRC: m_repr.pdi.swap(other.m_repr.pdi); break;
        default:     throw Object::wrong_type(m_repr_ix);
    }
}

inline
ItemIterator::ItemIterator(ItemIterator&& other) : m_repr_ix{other.m_repr_ix} {
    switch (m_repr_ix) {
        case ReprIX::NIL: break;
        case ReprIX::LIST: m_repr.li = other.m_repr.li; break;
        case ReprIX::SMAP:  m_repr.smi = other.m_repr.smi; break;
        case ReprIX::OMAP: m_repr.omi = other.m_repr.omi; break;
        case ReprIX::DSRC: m_repr.pdi = std::move(other).m_repr.pdi; break;
        default:     throw Object::wrong_type(m_repr_ix);
    }
}

inline
auto& ItemIterator::operator = (ItemIterator&& other) {
    m_repr_ix = other.m_repr_ix;
    switch (m_repr_ix) {
        case ReprIX::NIL: break;
        case ReprIX::LIST: m_repr.li = other.m_repr.li; break;
        case ReprIX::SMAP:  m_repr.smi = other.m_repr.smi; break;
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
        case ReprIX::SMAP:  ++(m_repr.smi); break;
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
        case ReprIX::SMAP: {
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
        case ReprIX::SMAP:  return m_repr.smi == other.m_repr.smi;
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
            if (m_slice.min().value() == nil) {
                return ItemIterator{0, list.begin()};
            } else {
                auto indices = m_slice.to_indices(list.size());
                auto start = std::get<0>(indices);
                return ItemIterator{start, list.begin() + start};
            }
        }
        case ReprIX::SMAP: {
            auto& map = std::get<0>(*m_obj.m_repr.psm);
            auto& min_key = m_slice.min().value();
            if (min_key == nil) {
                return ItemIterator{map.begin()};
            } else {
                auto it = map.lower_bound(min_key);
                if (m_slice.min().is_open() && it != map.end())
                    ++it;
                return ItemIterator{it};
            }
        }
        case ReprIX::OMAP: {
            if (!m_slice.is_empty()) throw WrongType(Object::type_name(repr_ix));
            return ItemIterator{std::get<0>(*m_obj.m_repr.pom).begin()};
        }
        case ReprIX::DSRC: {
            auto p_it = m_slice.is_empty()? m_obj.m_repr.ds->item_iter(): m_obj.m_repr.ds->item_iter(m_slice);
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
            if (m_slice.max().value() == nil) {
                return ItemIterator{(Int)list.size(), list.end()};
            } else {
                auto indices = m_slice.to_indices(list.size());
                auto stop = std::get<1>(indices);
                return ItemIterator{stop, list.begin() + stop};
            }
        }
        case ReprIX::SMAP: {
            auto& map = std::get<0>(*m_obj.m_repr.psm);
            auto& max_key = m_slice.max().value();
            if (max_key == nil) {
                return ItemIterator{map.end()};
            } else {
                auto it = map.upper_bound(max_key);
                if (m_slice.max().is_open())
                    --it;
                return ItemIterator{it};
            }
        }
        case ReprIX::OMAP: {
            if (!m_slice.is_empty()) throw WrongType(Object::type_name(repr_ix));
            return ItemIterator{std::get<0>(*m_obj.m_repr.pom).end()};
        }
        case ReprIX::DSRC: return ItemIterator{};
        default: throw Object::wrong_type(repr_ix);
    }
}
