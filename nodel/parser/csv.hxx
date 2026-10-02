/// @b License: @n Apache License v2.0
/// @copyright Robert Dunnagan
#pragma once

#include <nodel/core/Object.hxx>
#include <nodel/support/parse.hxx>

#include <fstream>

namespace nodel::csv {
namespace impl {

template <typename StreamType>
class Parser
{
  public:
    Parser(StreamType& stream) : m_it{stream} {}

    Object parse();

    size_t pos() const               { return m_it.consumed() - (m_unread.size() - m_unread_pos); }
    const std::string& error() const { return m_error; }

  private:
    bool parse_row(ObjectList&);
    bool parse_column(ObjectList&);
    bool parse_quoted(ObjectList&);
    void parse_single_quoted(ObjectList&);
    Object parse_unquoted();
    void consume_whitespace();
    void set_error(const std::string&);

    char peek()       { return m_unread_pos < m_unread.size() ? m_unread[m_unread_pos] : m_it.peek(); }
    void next()       { if (m_unread_pos < m_unread.size()) ++m_unread_pos; else m_it.next(); }
    bool done() const { return m_unread_pos == m_unread.size() && m_it.done(); }
    void unread(const std::string&);

  private:
    parse::StreamAdapter<StreamType> m_it;
    std::string m_unread;  // text to read again before resuming m_it
    size_t m_unread_pos = 0;
    std::string m_error;
};


template <typename StreamType>
Object Parser<StreamType>::parse() {
    ObjectList table;
    while (!done())
        if (!parse_row(table))
            return make_error(m_error);
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
        if (done()) {
            save_row(table, row);
            return true;
        }
        char c = peek();
        switch (c) {
            case 0:    [[fallthrough]];
            case '\n': next(); save_row(table, row); return true;
            case ',':  next(); break;
            default:   set_error("Expected comma or new-line"); return false;
        }
    } while (!done());

    return true;
}

template <typename StreamType>
bool Parser<StreamType>::parse_column(ObjectList& row) {
    consume_whitespace();
    if (done()) return false;
    char c = peek();
    switch (c) {
        case 0:    break;
        case ',':  row.push_back(""); break;
        case '"':  return parse_quoted(row);
        case '\'': parse_single_quoted(row); break;
        default:   {
            Object col = parse_unquoted();
            row.push_back(col);
            break;
        }
    }
    return true;
}

template <typename StreamType>
bool Parser<StreamType>::parse_quoted(ObjectList& row) {
    Object object{Object::STR};
    auto& str = object.as<String>();
    char quote = peek();
    next();
    // 0 is the end-of-input sentinel appended by StreamAdapter
    while (!done() && peek() != 0) {
        char c = peek();
        next();
        if (c == quote) {
            consume_whitespace();
            row.push_back(object);
            return true;
        }
        if (c == '\\') {
            if (done() || peek() == 0) break;
            c = peek();
            next();
        }
        str.push_back(c);
    }
    set_error("Unterminated quoted string");
    return false;
}

// A single-quoted field must close on the same line and be followed by a comma,
// a new-line, or the end of input. Otherwise the apostrophe is ordinary text, as in
// '90s or 'til, and the text scanned so far is read again as an unquoted field.
template <typename StreamType>
void Parser<StreamType>::parse_single_quoted(ObjectList& row) {
    Object object{Object::STR};
    auto& str = object.as<String>();
    std::string raw{"'"};
    next();
    while (!done() && peek() != 0 && peek() != '\n') {
        char c = peek();
        raw.push_back(c);
        next();
        if (c == '\'') {
            for (c = peek(); std::isspace(c) && c != '\n' && !done(); next(), c = peek())
                raw.push_back(c);
            if (done() || c == 0 || c == '\n' || c == ',') {
                row.push_back(object);
                return;
            }
            break;
        }
        if (c == '\\') {
            if (done() || peek() == 0 || peek() == '\n') break;
            c = peek();
            raw.push_back(c);
            next();
        }
        str.push_back(c);
    }
    unread(raw);
    row.push_back(parse_unquoted());
}

template <typename StreamType>
Object Parser<StreamType>::parse_unquoted() {
    Object object{Object::STR};
    auto& str = object.as<String>();
    for (char c = peek(); c != ',' && c != '\n' && c != 0 && !done(); next(), c = peek()) {
        str.push_back(c);
    }

    char first_char = str[0];
    if (std::isdigit(first_char) || first_char == '-' || first_char == '+') {
        char* beg = str.data();
        char* end = beg + str.size();
        if (str.find('.') != std::string::npos) {
            char* ptr = end;
            Float value = std::strtold(beg, &ptr);
            while (ptr != end && ptr != beg && std::isspace(*ptr)) ++ptr;
            if (ptr == end) return value;
        } else {
            Int value;
            auto [ptr, ec] = std::from_chars(beg, end, value);
            while (ptr != end && ptr != beg && std::isspace(*ptr)) ++ptr;
            if (ptr == end && ec == std::errc{}) return value;
        }
    }

    object.set(rtrim(str));
    return object;
}

template <typename StreamType>
void Parser<StreamType>::consume_whitespace() {
    for (char c = peek(); std::isspace(c) && c != '\n' && !done(); next(), c = peek());
}

template <typename StreamType>
void Parser<StreamType>::unread(const std::string& text) {
    m_unread = text + m_unread.substr(m_unread_pos);
    m_unread_pos = 0;
}

template <typename StreamType>
void Parser<StreamType>::set_error(const std::string& msg) {
    std::stringstream ss;
    ss << msg << " at pos=" << pos();
    m_error = ss.str();
}

} // namespace impl


inline
Object parse(std::string&& str) {
    std::istringstream in{std::forward<std::string>(str)};
    impl::Parser parser{in};
    return parser.parse();
}

inline
Object parse_file(const std::string& file_name) {
    std::ifstream f_in{file_name, std::ios::in};
    if (!f_in.is_open()) {
        std::stringstream ss;
        ss << "Error opening file: " << file_name;
        return make_error(ss.str());
    } else {
        impl::Parser parser{f_in};
        return parser.parse();
    }
}

} // namespace nodel::csv
