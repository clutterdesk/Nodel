#pragma once

#include <cstring>
#include <fmt/core.h>

namespace nodel {

class Oid
{
  public:
    Oid() : Oid(0, 0) {}
    Oid(uint8_t a, uint64_t b) {
        oid[0] = a;
        memcpy(oid + 1, &b, 8);
    }

    Oid(const Oid&) = default;
    Oid(Oid&&) = default;
    auto& operator = (const Oid& other) { std::memcpy(oid, other.oid, 9); return *this;}
    auto& operator = (Oid&& other) { std::memcpy(oid, other.oid, 9); return *this; }

    bool operator == (const Oid& other) const { return std::memcmp(oid, other.oid, 9) == 0; }

    std::string to_str() { return fmt::format("{:x}{:x}", *((uint64_t*)oid), oid[8]); }

    static Oid null() { return Oid{}; }
    static Oid illegal() { return Oid{0xFF, 0xFFFFFFFFFFFFFFFFULL}; }

  private:
    uint8_t oid[9];
};

} // namespace nodel
