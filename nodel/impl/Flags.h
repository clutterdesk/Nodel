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


template <typename T>
struct Flags
{
    constexpr Flags(T value) : m_value{value} {}

    Flags(const Flags&) = default;
    Flags<T>& operator = (const Flags<size_t>& flag) { m_value = flag.m_value; }

    Flags<T> operator |= (Flags flags) { return m_value |= flags.m_value; }
    Flags<T> operator &= (Flags flags) { return m_value |= flags.m_value; }
    Flags<T> operator ^= (Flags flags) { return m_value |= flags.m_value; }

    bool operator ! () const { return m_value == 0; }
    Flags<T> operator ~ () const { return ~m_value; }

    operator T () const { return (T)m_value; }
    operator bool () const { return (bool)m_value; }

  protected:
    T m_value;

  template <typename V> friend Flags<V> operator | (Flags<V>, Flags<V>);
  template <typename V> friend Flags<V> operator & (Flags<V>, Flags<V>);
  template <typename V> friend Flags<V> operator ^ (Flags<V>, Flags<V>);
};

template <typename T>
Flags<T> operator | (Flags<T> l, Flags<T> r) { return l.m_value | r.m_value; }

template <typename T>
Flags<T> operator & (Flags<T> l, Flags<T> r) { return l.m_value & r.m_value; }

template <typename T>
Flags<T> operator ^ (Flags<T> l, Flags<T> r) { return l.m_value ^ r.m_value; }

#define FLAG8 constexpr static Flags<uint8_t>
#define FLAG16 constexpr static Flags<uint16_t>
#define FLAG32 constexpr static Flags<uint32_t>
#define FLAG64 constexpr static Flags<uint64_t>
