/// @b License: @n Apache License v2.0
/// @copyright Robert Dunnagan
#pragma once

#include <nodel/core/Object.h>
#include <nodel/support/parse.h>

#include <fstream>

namespace nodel::csv {
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
    bool parse_row(ObjectList&);
    bool parse_column(ObjectList&);
    Object parse_quoted();
    Object parse_unquoted();
    void consume_whitespace();
    void set_error(const std::string&);

  private:
    nodel::parse::StreamAdapter<StreamType> m_it;
    std::string m_error;
};


template <typename StreamType>
Object Parser<StreamType>::parse() {
    ObjectList table;
    while (!m_it.done())
        if (!parse_row(table))
            return nil;
    return table;
}

inline
void save_row(ObjectList& table, ObjectList& row) {
    if (row.size() == 1) {
        auto col = row[0];
        if (col.is_type<String>() && col.size() == 0)
            row.clear();
    }

    if (row.size() > 0) {
        table.push_back(row);
    }
}

template <typename StreamType>
bool Parser<StreamType>::parse_row(ObjectList& table) {
    ObjectList row;

    do {
        if (!parse_column(row)) return false;
        consume_whitespace();
        if (m_it.done()) {
            save_row(table, row);
            return true;
        }
        char c = m_it.peek();
        switch (c) {
            case 0:    [[fallthrough]];
            case '\n': m_it.next(); save_row(table, row); return true;
            case ',':  m_it.next(); break;
            default:   set_error("Expected comma or new-line"); return false;
        }
    } while (!m_it.done());

    return true;
}

template <typename StreamType>
bool Parser<StreamType>::parse_column(ObjectList& row) {
    consume_whitespace();
    if (m_it.done()) return false;
    char c = m_it.peek();
    switch (c) {
        case 0:    break;
        case ',':  row.push_back(""); break;
        case '"':  [[fallthrough]];
        case '\'': {
            Object col = parse_quoted();
            row.push_back(col);
            break;
        }
        default:   {
            Object col = parse_unquoted();
            row.push_back(col);
            break;
        }
    }
    return true;
}

template <typename StreamType>
Object Parser<StreamType>::parse_quoted() {
    Object object{Object::STR};
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
    Object object{Object::STR};
    auto& str = object.as<String>();
    for (char c = m_it.peek(); c != ',' && c != '\n' && c != 0 && !m_it.done(); m_it.next(), c = m_it.peek()) {
        str.push_back(c);
    }

    char first_char = str[0];
    if (std::isdigit(first_char) || first_char == '-' || first_char == '+') {
        if (str.find('.') != std::string::npos) {
            return str_to_float(str);
        } else {
            Int value;
            const char* beg = str.data();
            const char* end = beg + str.size();
            auto [ptr, ec] = std::from_chars(beg, end, value);
            if (ec == std::errc{}) return value;
        }
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
    ss << msg << " at pos=" << m_it.consumed();
    m_error = ss.str();
}

} // namespace impl


struct ParseError
{
    size_t error_offset = 0;
    std::string error_message;

    std::string to_str() const {
        if (error_message.size() > 0) {
            std::stringstream ss;
            ss << "CSV parse error at " << error_offset << ": " << error_message;
            return ss.str();
        }
        return "";
    }
};


inline
Object parse(std::string&& str, std::optional<ParseError>& error) {
    std::istringstream in{std::forward<std::string>(str)};
    impl::Parser parser{in};
    Object result = parser.parse();
    if (result == nil) {
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
        return nil;
    } else {
        impl::Parser parser{f_in};
        Object result = parser.parse();
        if (result == nil) {
            ParseError parse_error{parser.pos(), std::move(parser.error())};
            error = parse_error.to_str();
        }
        return result;
    }
}

} // namespace nodel::csv
