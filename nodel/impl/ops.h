#pragma once

#include "types.h"

namespace nodel {

template <typename T> bool is_null(const T&);
template <typename T> bool is_bool(const T&);
template <typename T> bool is_int(const T&);
template <typename T> bool is_uint(const T&);
template <typename T> bool is_float(const T&);
template <typename T> bool is_str(const T&);
template <typename T> bool is_num(const T&);

template <typename T> bool to_bool(const T&);
template <typename T> Int to_int(const T&);
template <typename T> UInt to_uint(const T&);
template <typename T> Float to_float(const T&);
template <typename T> std::string to_str(const T&);
template <typename T> std::string to_json(const T&);

} // namespace nodel
