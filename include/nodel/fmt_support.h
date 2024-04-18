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
