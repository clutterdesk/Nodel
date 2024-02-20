#pragma once

#include "impl/Object.h"
#include "impl/json.h"

namespace nodel {

inline
Object parse_json(const std::string& json, std::string& error) {
    std::optional<nodel::json::ParseError> parse_error;
    Object result = nodel::json::parse(std::string(json), parse_error);
    if (parse_error) {
        error = parse_error->to_str();
        return {};
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

inline
Object::Object(Json&& json) : fields{EMPTY_I} {
    std::optional<nodel::json::ParseError> parse_error;
    *this = json::parse(std::string(std::move(json.text)), parse_error);
    if (parse_error)
        throw json::JsonException(parse_error->to_str());
}

} // namespace nodel
