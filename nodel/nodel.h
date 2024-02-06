#pragma once

#include "impl/Object.h"
#include "impl/Node.h"
#include "impl/json.h"
#include "impl/except.h"

namespace nodel {

inline
Node Node::from_json(const std::string& json, std::string& error) {
    std::optional<nodel::json::ParseError> parse_error;
    Object result = nodel::json::parse(json, parse_error);
    if (parse_error) {
        error = parse_error->to_str();
        return {};
    }
    return Node{result};
}

inline
Node Node::from_json(const std::string& json) {
    std::optional<nodel::json::ParseError> parse_error;
    Object result = nodel::json::parse(json, parse_error);
    if (parse_error) {
        throw JsonException(parse_error->to_str());
        return {};
    }
    return Node{result};
}

} // namespace nodel
