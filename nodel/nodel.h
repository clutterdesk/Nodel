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

#include "core/Object.h"
#include "serialization/json.h"

namespace nodel {

inline
Object parse_json(const std::string& json, std::string& error) {
    std::optional<nodel::json::ParseError> parse_error;
    Object result = nodel::json::parse(std::string(json), parse_error);
    if (parse_error) {
        error = parse_error->to_str();
        return null;
    }
    return Object{result};
}

inline
Object parse_json(const std::string& json) {
    std::optional<nodel::json::ParseError> parse_error;
    Object result = nodel::json::parse(std::string(json), parse_error);
    if (parse_error)
        throw json::JsonException(parse_error->to_str());
    return Object{result};
}

inline
Object parse_json(std::string&& json, std::string& error) {
    std::optional<nodel::json::ParseError> parse_error;
    Object result = nodel::json::parse(std::forward<std::string>(json), parse_error);
    if (parse_error) {
        error = parse_error->to_str();
        return {};
    }
    return Object{result};
}

inline
Object parse_json(std::string&& json) {
    std::optional<nodel::json::ParseError> parse_error;
    Object result = nodel::json::parse(std::forward<std::string>(json), parse_error);
    if (parse_error) {
        throw json::JsonException(parse_error->to_str());
        return {};
    }
    return Object{result};
}

} // namespace nodel
