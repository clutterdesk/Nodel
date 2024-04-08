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
