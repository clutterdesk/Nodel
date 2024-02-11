#pragma once

#include <cstring>
#include <fmt/core.h>

namespace nodel {

class Oid
{
  public:
    Oid() : Oid(0, 0) {}
    Oid(uint8_t a, uint64_t b) : b{b}, a{a} {}

    Oid(const Oid&) = default;
    Oid(Oid&&) = default;

    auto& operator = (const Oid& other) { b = other.b; a = other.a; return *this; }
    auto& operator = (Oid&& other)      { b = other.b; a = other.a; return *this; }

    bool operator == (const Oid& other) const { return b == other.b && a == other.a; }

    std::string to_str() { return fmt::format("{:x}/{:016x}", a, b); }

    static Oid null() { return Oid{}; }
    static Oid illegal() { return Oid{0xFF, 0xFFFFFFFFFFFFFFFFULL}; }

  private:
    uint64_t b;
    uint8_t a;
};

} // namespace nodel
