#pragma once

#include "Key.h"
#include "Object.h"
#include <nodel/support/string.h>
#include <nodel/parser/json.h>

#include <sstream>

namespace nodel {

inline
std::string serialize(const Key& key) {
    switch (key.type()) {
        case Key::NIL:  return "0";
        case Key::BOOL:  return key.as<bool>()? "2": "1";
        case Key::INT:   return "3" + key.to_str();
        case Key::UINT:  return "4" + key.to_str();
        case Key::FLOAT: return "5" + key.to_str();
        case Key::STR:   return "6" + key.to_str();
        default:           throw Key::wrong_type(key.type());
    }
}

inline
bool deserialize(const std::string_view& data, Key& key) {
    if (data.size() < 1) return false;
    switch (data[0]) {
        case '0': key = nil; break;
        case '1': key = false; break;
        case '2': key = true; break;
        case '3': key = str_to_int(data.substr(1)); break;
        case '4': key = str_to_uint(data.substr(1)); break;
        case '5': key = str_to_float(data.substr(1)); break;
        case '6': key = data.substr(1); break;
        default:  return false;
    }
    return true;
}

inline
std::string serialize(const Object& value) {
    switch (value.type()) {
        case Object::NIL:  return "0";
        case Object::BOOL:  return value.as<bool>()? "2": "1";
        case Object::INT:   return '3' + value.to_str();
        case Object::UINT:  return '4' + value.to_str();
        case Object::FLOAT: return '5' + value.to_str();
        case Object::STR:   return '6' + value.as<String>();
        case Object::LIST:  [[fallthrough]];
        case Object::OMAP:  return '7' + value.to_json();
        default:              throw Object::wrong_type(value.type());
    }
}

inline
bool deserialize(const std::string_view& data, Object& value) {
    if (data.size() < 1) return false;
    switch (data[0]) {
        case '0': value = nil; break;
        case '1': value = false; break;
        case '2': value = true; break;
        case '3': value = str_to_int(data.substr(1)); break;
        case '4': value = str_to_uint(data.substr(1)); break;
        case '5': value = str_to_float(data.substr(1)); break;
        case '6': value = data.substr(1); break;
        case '7': value = json::parse(data.substr(1)); break;
        default:  return false;
    }
    return true;
}

}  // nodel namespace

