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

namespace nodel {

#ifndef NODEL_ARCH
#if _WIN32 || _WIN64
#if _WIN64
#define NODEL_ARCH 64
#else
#define NODEL_ARCH 32
#endif
#endif

#if __GNUC__
#if __x86_64__ || __ppc64__
#define NODEL_ARCH 64
#else
#define NODEL_ARCH 32
#endif
#endif
#endif // NODEL_ARCH

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

struct null_t {};
constexpr static null_t none;

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
concept is_number = is_integral<T> || is_like_Float<T>;

template <typename T>
concept is_byvalue = std::is_same<T, bool>::value || is_number<T>;

template <typename T>
concept is_like_string = std::is_same<typename std::decay<T>::type, char*>::value ||
                         std::is_same<typename std::decay<T>::type, const char*>::value ||
                         std::is_same<T, String>::value ||
                         std::is_same<T, StringView>::value;

} // namespace nodel
