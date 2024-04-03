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

#include "Key.h"
#include "Object.h"
#include <nodel/support/string.h>
#include <nodel/format/json.h>

#include <sstream>

namespace nodel {

inline
std::string serialize(const Key& key) {
    switch (key.type()) {
        case Key::NULL_I:  return "0";
        case Key::BOOL_I:  return key.as<bool>()? "2": "1";
        case Key::INT_I:   return "3" + key.to_str();
        case Key::UINT_I:  return "4" + key.to_str();
        case Key::FLOAT_I: return "5" + key.to_str();
        case Key::STR_I:   return "6" + key.to_str();
        default:           throw Key::wrong_type(key.type());
    }
}

inline
bool deserialize(const std::string_view& data, Key& key) {
    if (data.size() < 1) return false;
    switch (data[0]) {
        case '0': key = null; break;
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
        case Object::NULL_I:  return "0";
        case Object::BOOL_I:  return value.as<bool>()? "2": "1";
        case Object::INT_I:   return '3' + value.to_str();
        case Object::UINT_I:  return '4' + value.to_str();
        case Object::FLOAT_I: return '5' + value.to_str();
        case Object::STR_I:   return '6' + value.as<String>();
        case Object::LIST_I:  [[fallthrough]];
        case Object::OMAP_I:  return '7' + value.to_json();
        default:              throw Object::wrong_type(value.type());
    }
}

inline
bool deserialize(const std::string_view& data, Object& value) {
    if (data.size() < 1) return false;
    switch (data[0]) {
        case '0': value = null; break;
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

