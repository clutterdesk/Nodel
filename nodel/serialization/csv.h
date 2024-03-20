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
#include <nodel/support/StreamIterator.h>

#include <fstream>

namespace nodel {
namespace csv {

class exception : public std::exception
{
  public:
    exception(const std::string& msg) : msg(msg) {}
    const char* what() const throw() override { return msg.c_str(); }

  private:
    const std::string msg;
};

namespace impl {

template <typename StreamType>
class Parser
{
  public:
    Parser(StreamType& stream) : m_it{stream} {}

    Object parse();

    size_t pos() const               { return m_it.consumed(); }
    const std::string& error() const { return m_error; }

  private:
    bool parse_row(List&);
    bool parse_column(List&);
    Object parse_quoted();
    Object parse_unquoted();
    void consume_whitespace();
    void set_error(const std::string&);

  private:
    nodel::impl::StreamIterator<StreamType> m_it;
    std::string m_error;
};


template <typename StreamType>
Object Parser<StreamType>::parse() {
    List table;
    while (!m_it.done())
        if (!parse_row(table))
            return null;
    return table;
}

template <typename StreamType>
bool Parser<StreamType>::parse_row(List& table) {
    List row;

    do {
        if (!parse_column(row)) return false;
        consume_whitespace();
        if (m_it.done()) {
            if (row.size() > 0) table.push_back(row);
            return true;
        }
        char c = m_it.peek();
        switch (c) {
            case 0:    [[fallthrough]];
            case '\n': m_it.next(); if (row.size() > 0) table.push_back(row); return true;
            case ',':  m_it.next(); break;
            default:   set_error("Expected comma or new-line"); return false;
        }
    } while (!m_it.done());

    return true;
}

template <typename StreamType>
bool Parser<StreamType>::parse_column(List& row) {
    consume_whitespace();
    if (m_it.done()) return false;
    char c = m_it.peek();
    switch (c) {
        case 0:    break;
        case ',':  row.push_back(""); m_it.next(); break;
        case '"':  [[fallthrough]];
        case '\'': {
            Object col = parse_quoted();
            row.push_back(col);
            break;
        }
        default:   {
            Object col = parse_unquoted();
            if (col.size() > 0) row.push_back(col);
            break;
        }
    }
    return true;
}

template <typename StreamType>
Object Parser<StreamType>::parse_quoted() {
    Object object{Object::STR_I};
    auto& str = object.as<String>();
    char quote = m_it.peek();
    m_it.next();
    while (!m_it.done()) {
        char c = m_it.peek();
        if (c == '\\') {
            m_it.next();
            c = m_it.peek();
            str.push_back(c);
        } else if (c == quote) {
            m_it.next();
            break;
        } else {
            str.push_back(c);
        }
        m_it.next();
    }
    consume_whitespace();
    return object;
}

template <typename StreamType>
Object Parser<StreamType>::parse_unquoted() {
    Object object{Object::STR_I};
    auto& str = object.as<String>();
    for (char c = m_it.peek(); c != ',' && c != '\n' && c != 0 && !m_it.done(); m_it.next(), c = m_it.peek()) {
        str.push_back(c);
    }
    return object;
}

template <typename StreamType>
void Parser<StreamType>::consume_whitespace() {
    for (char c = m_it.peek(); std::isspace(c) && c != '\n' && !m_it.done(); m_it.next(), c = m_it.peek());
}

template <typename StreamType>
void Parser<StreamType>::set_error(const std::string& msg) {
    std::stringstream ss;
    ss << msg << " at m_pos=" << m_it.consumed();
    m_error = ss.str();
}

} // namespace impl


struct ParseError
{
    size_t error_offset = 0;
    std::string error_message;

    std::string to_str() const {
        if (error_message.size() > 0)
            return fmt::format("CSV parse error at {}: {}", error_offset, error_message);
        return "";
    }
};


inline
Object parse(std::string&& str, std::optional<ParseError>& error) {
    std::istringstream in{std::forward<std::string>(str)};
    impl::Parser parser{in};
    Object result = parser.parse();
    if (result.is_null()) {
        error = ParseError{parser.pos(), std::move(parser.error())};
    }
    return result;
}

inline
Object parse(std::string&& str, std::string& error) {
    std::optional<ParseError> parse_error;
    Object result = parse(std::forward<std::string>(str), parse_error);
    if (parse_error) {
        error = parse_error->to_str();
    }
    return result;
}

inline
Object parse(std::string&& str) {
    std::istringstream in{std::forward<std::string>(str)};
    impl::Parser parser{in};
    return parser.parse();
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
        impl::Parser parser{f_in};
        Object result = parser.parse();
        if (result.is_null()) {
            ParseError parse_error{parser.pos(), std::move(parser.error())};
            error = parse_error.to_str();
        }
        return result;
    }
}

} // namespace csv
} // namespace nodel
