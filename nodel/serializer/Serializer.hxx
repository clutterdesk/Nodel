/// @b License: @n Apache License v2.0
/// @copyright Robert Dunnagan
#pragma once

#include <nodel/parser/csv.hxx>
#include <nodel/core/Object.hxx>
#include <nodel/support/Ref.hxx>

#include <iostream>

namespace nodel {

class Serializer
{
  public:
    Serializer() = default;
    Serializer(Object::ReprIX repr_ix) : m_repr_ix{repr_ix} {}
    virtual ~Serializer() = default;

    virtual Object read(std::istream&, size_t size_hint) = 0;
    virtual void write(std::ostream&, const Object&) = 0;

    Object::ReprIX get_repr_ix() const { return m_repr_ix; }

  private:
    Object::ReprIX m_repr_ix = Object::EMPTY;
    refcnt_t m_ref_count;

  template <typename> friend class ::nodel::Ref;
};

} // namespace nodel::serial
