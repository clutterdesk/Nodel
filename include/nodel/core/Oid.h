/// @b License: @n Apache License v2.0
/// @copyright Robert Dunnagan
#pragma once

#include <cstring>
#include <sstream>

namespace nodel {

class Oid
{
  public:
    Oid() : Oid(0, 0) {}
    Oid(uint8_t a, uint64_t b) : m_b{b}, m_a{a} {}

    Oid(const Oid&) = default;
    Oid(Oid&&) = default;

    auto& operator = (const Oid& other) { m_b = other.m_b; m_a = other.m_a; return *this; }
    auto& operator = (Oid&& other)      { m_b = other.m_b; m_a = other.m_a; return *this; }

    bool operator == (const Oid& other) const { return m_b == other.m_b && m_a == other.m_a; }

    std::string to_str() {
        std::stringstream ss;
        ss << std::hex << m_a << std::setw(16) << std::setfill('0') << m_b;
        return ss.str();
    }

    size_t hash() const { return m_b ^ (((uint64_t)m_a) << 56); }

    static Oid nil() { return Oid{}; }
    static Oid illegal() { return Oid{0xFF, 0xFFFFFFFFFFFFFFFFULL}; }

  private:
    uint64_t m_b;
    uint8_t m_a;
};

} // namespace nodel


namespace std {

//----------------------------------------------------------------------------------
// std::hash
//----------------------------------------------------------------------------------
template<>
struct hash<nodel::Oid>
{
    std::size_t operator () (const nodel::Oid& oid) const noexcept {
      return oid.hash();
    }
};

} // namespace std
