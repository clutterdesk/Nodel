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

#include <nodel/core/Object.h>
#include <nodel/support/parse.h>
#include <nodel/support/exception.h>

#include <ctype.h>
#include <cstdlib>
#include <cerrno>
#include <sstream>
#include <fstream>

namespace nodel {
namespace json {

class JsonException : public NodelException
{
  public:
    JsonException(const std::string& msg) : msg(msg) {}
    const char* what() const throw() override { return msg.c_str(); }

  private:
    const std::string msg;
};


namespace impl {

template <typename StreamType>
struct Parser
{
  public:
    Parser(const StreamType& stream) : m_it{stream} {}

    Parser(Parser&&) =  default;
    Parser(const Parser&) = delete;
    auto operator = (Parser&&) = delete;
    auto operator = (const Parser&) = delete;

    Object::ReprType parse_type();  // quickly determine type without full parse
    bool parse_object();
    bool parse_object(char term_char);
    bool parse_number();
    bool parse_string();
    bool parse_map();
    bool parse_list();

    template <typename T>
    bool expect(const char* seq, T value);

    void consume_whitespace();

    void create_error(const std::string& message);

    StreamType m_it;
    Object m_curr;
    std::string m_scratch{32};
    size_t m_error_offset = 0;
    std::string m_error_message;
};

template <typename StreamType>
Object::ReprType Parser<StreamType>::parse_type() {
    consume_whitespace();
    switch (m_it.peek()) {
        case '{': return Object::OMAP_I;
        case '[': return Object::LIST_I;
        case 'n': return Object::NULL_I;
        case 't':
        case 'f': return Object::BOOL_I;
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
        case '+':
        case '-':
        case '.':
            if (parse_number())
                return m_curr.type();
            break;
        case '"':
        case '\'':
            return Object::STR_I;
        default:
            break;
    }
    return Object::BAD_I;  // TODO: BAD is not supported
}

template <typename StreamType>
bool Parser<StreamType>::parse_object()
{
    if (!parse_object('\0')) {
        if (m_error_message.size() == 0) {
            m_error_message = "No object in json stream";
        }
        return false;
    }
    return true;
}

template <typename StreamType>
bool Parser<StreamType>::parse_object(char term_char)
{
    for (; !m_it.done(); m_it.next()) {
        switch (m_it.peek())
        {
            case ' ':
            case '\t':
            case '\r':
            case '\n':
                continue;

            case '-':
            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
                return parse_number();

            case '\'':
            case '"':
                return parse_string();

            case '[': return parse_list();
            case '{': return parse_map();

            case 't': return expect("true", true);
            case 'f': return expect("false", false);
            case 'n': return expect("null", null);

            default:
                return m_it.peek() == term_char;
        }
    }
    return false;
}

template <typename StreamType>
bool Parser<StreamType>::parse_number() {
    m_scratch.clear();

    bool is_done = false;
    bool is_float = false;
    for (; !m_it.done(); m_it.next()) {
        char c = m_it.peek();
        switch (c) {
            case '+':
            case '-':
            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
                break;
            case '.':
            case 'e':
            case 'E':
                is_float = true;
                break;
            default:
                is_done = true;
                break;
        }
        if (is_done) break;
        m_scratch.push_back(c);
    }

    const char* str = m_scratch.c_str();
    const char* scratch_end = str + m_scratch.size();
    char* end = 0;
    if (is_float) {
        m_curr.refer_to(Object{strtod(str, &end)});
    } else {
        errno = 0;
        m_curr.refer_to(Object{strtoll(str, &end, 10)});
        if (errno) {
            errno = 0;
            m_curr.refer_to(Object{strtoull(str, &end, 10)});
        }
    }

    m_scratch.clear();

    if (errno) {
        create_error(strerror(errno));
        errno = 0;
        return false;
    } else if (end != scratch_end) {
        create_error("Numeric syntax error");
        return false;
    } else {
        return true;
    }
}

template <typename StreamType>
bool Parser<StreamType>::parse_string() {
    char quote = m_it.peek();
    m_it.next();
    bool escape = false;
    std::string str;
    for(; !m_it.done(); m_it.next()) {
        char c = m_it.peek();
        if (c == '\\') {
            escape = true;
        } else if (!escape) {
            if (c == quote) { m_it.next(); quote = 0; break; }
            str.push_back(c);
        } else {
            escape = false;
            str.push_back(c);
        }
    }

    if (quote != 0) {
        create_error("Unterminated string");
        return false;
    }

    m_curr.refer_to(std::move(str));
    return true;
}

template <typename StreamType>
bool Parser<StreamType>::parse_list() {
    List list;
    m_it.next();  // consume [
    consume_whitespace();
    if (m_it.peek() == ']') {
        m_it.next();
        m_curr.refer_to(std::move(list));
        return true;
    }
    while (!m_it.done()) {
        if (!parse_object(']')) {
            create_error("Expected value or object");
            return false;
        }
        list.push_back(m_curr);
        consume_whitespace();
        char c = m_it.peek();
        if (c == ']') {
            m_it.next();
            m_curr.refer_to(std::move(list));
            return true;
        } else if (c == ',') {
            m_it.next();
            continue;
        }
    }

    create_error("Unterminated list");
    return false;
}

template <typename StreamType>
bool Parser<StreamType>::parse_map() {
    Map map;
    m_it.next();  // consume {
    consume_whitespace();
    if (m_it.peek() == '}') {
        m_it.next();
        m_curr.refer_to(std::move(map));
        return true;
    }

    while (!m_it.done()) {
        // key
        if (!parse_object(':')) {
            create_error("Expected dictionary key");
            return false;
        }

        Key key = m_curr.into_key();

        if (m_curr.is_container()) {
            create_error("Map keys must be a primitive type");
            return false;
        }

        consume_whitespace();
        char c = m_it.peek();
        if (c != ':') {
            create_error("Expected token ':'");
            return false;
        }

        // consume :
        m_it.next();

        // value
        if (!parse_object('}')) {
            create_error("Expected dictionary value or object");
            return false;
        }

        map.emplace(key, m_curr);
        consume_whitespace();

        c = m_it.peek();
        if (c == '}') {
            m_it.next();
            m_curr.refer_to(std::move(map));
            return true;
        } else if (c == ',') {
            m_it.next();
            continue;
        }
    }

    create_error("Unterminated map");
    return false;
}

template <typename StreamType>
template <typename T>
bool Parser<StreamType>::expect(const char* seq, T value) {
    const char* seq_it = seq;
    for (; *seq_it != 0 && !m_it.done(); m_it.next(), seq_it++) {
        if (*seq_it != m_it.peek()) {
            create_error("Invalid literal");
            return false;
        }
    }
    m_curr.refer_to(value);
    return true;
}

template <typename StreamType>
void Parser<StreamType>::consume_whitespace()
{
    while (!m_it.done() && std::isspace(m_it.peek())) m_it.next();
}

template <typename StreamType>
void Parser<StreamType>::create_error(const std::string& message)
{
    m_error_message = message;
    m_error_offset = m_it.consumed();
}

} // namespace impl


struct Error
{
    size_t error_offset = 0;
    std::string error_message;

    std::string to_str() const {
        if (error_message.size() > 0)
            return fmt::format("JSON parse error at {}: {}", error_offset, error_message);
        return "";
    }
};


inline
Object parse(const std::string_view& str, std::optional<Error>& error) {
    impl::Parser parser{nodel::impl::StringStreamAdapter{str}};
    if (!parser.parse_object()) {
        error = Error{parser.m_error_offset, std::move(parser.m_error_message)};
        return null;
    }
    return parser.m_curr;
}

inline
Object parse(const std::string_view& str, std::string& error) {
    std::optional<nodel::json::Error> parse_error;
    Object result = parse(str, parse_error);
    if (parse_error) {
        error = parse_error->to_str();
        return null;
    }
    return result;
}

inline
Object parse(const std::string_view& str) {
    impl::Parser parser{nodel::impl::StringStreamAdapter{str}};
    if (!parser.parse_object()) {
        throw SyntaxError(str, parser.m_error_offset, parser.m_error_message);
    }
    return parser.m_curr;
}


inline
Object parse_file(const std::string& file_name, std::string& error) {
    std::ifstream f_in{file_name, std::ios::in};
    if (!f_in.is_open()) {
        std::stringstream ss;
        ss << "Error opening file: " << file_name;
        error = ss.str();
        return null;
    } else {
        impl::Parser parser{nodel::impl::StreamAdapter{f_in}};
        if (!parser.parse_object()) {
            Error parse_error{parser.m_error_offset, std::move(parser.m_error_message)};
            error = parse_error.to_str();
            return null;
        }
        return parser.m_curr;
    }
}

} // namespace json
} // namespace nodel
