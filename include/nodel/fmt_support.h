#pragma once

#include <fmt/format.h>
#include <nodel/core/Key.h>
#include <nodel/core/Object.h>

template <>
struct fmt::formatter<nodel::Key> : formatter<std::string> {
  auto format(const nodel::Key& key, format_context& ctx) {
    return formatter<std::string>::format(key.to_str(), ctx);
  }
};

template <>
struct fmt::formatter<nodel::Object> : formatter<std::string> {
  auto format(const nodel::Object& obj, format_context& ctx) {
    return formatter<std::string>::format(obj.to_str(), ctx);
  }
};

template <>
struct fmt::formatter<nodel::OPath> : formatter<std::string> {
  auto format(const nodel::OPath& path, format_context& ctx) {
    return formatter<std::string>::format(path.to_str(), ctx);
  }
};

template <>
struct fmt::formatter<nodel::Object::Subscript<nodel::Key>> : formatter<std::string> {
  auto format(const nodel::Object::Subscript<nodel::Key>& obj, format_context& ctx) {
    return formatter<std::string>::format(obj.to_str(), ctx);
  }
};

template <>
struct fmt::formatter<nodel::Object::Subscript<nodel::OPath>> : formatter<std::string> {
  auto format(const nodel::Object::Subscript<nodel::OPath>& obj, format_context& ctx) {
    return formatter<std::string>::format(obj.to_str(), ctx);
  }
};
