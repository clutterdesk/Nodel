/// @b License: @n Apache License v2.0
/// @copyright Robert Dunnagan
#pragma once

#include <cstdint>
#include <string>
#include <sstream>

namespace nodel {

// Decided by pointer size rather than by architecture macro, so that every
// 64-bit target (e.g. aarch64, riscv64) is covered.
#ifndef NODEL_ARCH
#if UINTPTR_MAX == 0xFFFFFFFFu
#define NODEL_ARCH 32
#else
#define NODEL_ARCH 64
#endif
#endif // NODEL_ARCH

// Prevents inlining of a function, e.g. to keep a rarely taken path out of a
// hot inlined caller.  Place before the function's declaration specifiers.
#if defined(_MSC_VER) && !defined(__clang__)
#define NODEL_NOINLINE __declspec(noinline)
#else
#define NODEL_NOINLINE [[gnu::noinline]]
#endif

// Forces inlining of a function on a hot path.  Use in place of the `inline`
// keyword, since __forceinline replaces it.
#if defined(_MSC_VER) && !defined(__clang__)
#define NODEL_ALWAYS_INLINE __forceinline
#else
#define NODEL_ALWAYS_INLINE [[gnu::always_inline]] inline
#endif

// Marks a function as rarely called, e.g. one that only throws.  MSVC has no
// equivalent, so it expands to nothing there.
#if defined(_MSC_VER) && !defined(__clang__)
#define NODEL_COLD
#else
#define NODEL_COLD [[gnu::cold]]
#endif

#if NODEL_ARCH == 32
using refcnt_t = uint32_t;                     // only least-significant 28-bits used
#else
using refcnt_t = uint64_t;                     // only least-significant 56-bits used
#endif

using Int = int64_t;
using UInt = uint64_t;
using Float = double;
using String = std::string;
using StringView = std::string_view;
using StringStream = std::stringstream;

namespace python {
struct Support;
}  // namespace python

struct nil_t {};
constexpr static nil_t nil;

template <typename T>
concept is_bool = std::is_same<T, bool>::value;

template<typename T>
concept is_like_Int = std::is_signed<T>::value && std::is_integral<T>::value && std::is_convertible_v<T, Int>;

template<typename T>
concept is_like_UInt = !is_bool<T> && std::is_unsigned<T>::value && std::is_integral<T>::value && std::is_convertible_v<T, UInt>;

template<typename T>
concept is_integral = std::is_integral<T>::value;

template<typename T>
concept is_like_Float = std::is_floating_point<T>::value;

template<typename T>
concept is_numeric = is_integral<T> || is_like_Float<T>;

template <typename T>
concept is_byvalue = std::is_same<T, bool>::value || is_numeric<T>;

template <typename T>
concept is_like_string = std::is_same<typename std::decay<T>::type, char*>::value ||
                         std::is_same<typename std::decay<T>::type, const char*>::value ||
                         std::is_same<T, String>::value ||
                         std::is_same<T, StringView>::value;

} // namespace nodel
