#pragma once

#include <fmt/format.h>
#include <nodel/impl/Key.h"
#include <nodel/impl/Object.h"

template <>
struct fmt::formatter<Key> : formatter<std::string> {
  auto format(const Key& key, format_context& ctx) {
    return formatter<std::string>::format(key.to_str(), ctx);
  }
};

template <>
struct fmt::formatter<Object> : formatter<std::string> {
  auto format(const Object& obj, format_context& ctx) {
    return formatter<std::string>::format(obj.to_str(), ctx);
  }
};
