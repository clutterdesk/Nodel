#pragma once

#include <cstring>
#include <fmt/core.h>

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

    std::string to_str() { return fmt::format("{:x}/{:016x}", m_a, m_b); }

    static Oid null() { return Oid{}; }
    static Oid illegal() { return Oid{0xFF, 0xFFFFFFFFFFFFFFFFULL}; }

  private:
    uint64_t m_b;
    uint8_t m_a;
};

} // namespace nodel
