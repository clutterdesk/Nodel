/// @b License: @n Apache License v2.0
/// @copyright Robert Dunnagan
#pragma once

#include <fmt/format.h>
#include <nodel/core/Key.hxx>
#include <nodel/core/Object.hxx>

namespace fmt {

template <>
struct formatter<nodel::Key> : formatter<std::string> {
  auto format(const nodel::Key& key, format_context& ctx) const {
    return formatter<std::string>::format(key.to_str(), ctx);
  }
};

template <>
struct formatter<nodel::Object> : formatter<std::string> {
  auto format(const nodel::Object& obj, format_context& ctx) const {
    return formatter<std::string>::format(obj.to_str(), ctx);
  }
};

template <>
struct formatter<nodel::OPath> : formatter<std::string> {
  auto format(const nodel::OPath& path, format_context& ctx) const {
    return formatter<std::string>::format(path.to_str(), ctx);
  }
};

template <>
struct formatter<nodel::Object::Subscript<nodel::Key>> : formatter<std::string> {
  auto format(const nodel::Object::Subscript<nodel::Key>& obj, format_context& ctx) const {
    return formatter<std::string>::format(obj.to_str(), ctx);
  }
};

template <>
struct formatter<nodel::Object::Subscript<nodel::OPath>> : formatter<std::string> {
  auto format(const nodel::Object::Subscript<nodel::OPath>& obj, format_context& ctx) const {
    return formatter<std::string>::format(obj.to_str(), ctx);
  }
};

} // namespace fmt
