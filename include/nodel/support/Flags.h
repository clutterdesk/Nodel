#pragma once


template <typename T>
struct Flags
{
    constexpr Flags(T value) : m_value{value} {}

    Flags<T>& operator = (const Flags<size_t>& flag) { m_value = flag.m_value; }
    bool operator == (const Flags& flags) { return m_value == flags.m_value; }

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
