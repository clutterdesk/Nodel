/// @b License: @n Apache License v2.0
/// @copyright Robert Dunnagan
#pragma once

#include <nodel/core/Object.hxx>
#include <nodel/support/parse.hxx>
#include <nodel/support/exception.hxx>

#include <cstdint>
#include <cstdlib>
#include <cerrno>
#include <sstream>
#include <fstream>

namespace nodel {
namespace json {

struct Options
{
    bool use_sorted_map = false;
};


namespace impl {

inline
void append_utf8(std::string& str, uint32_t code) {
    if (code < 0x80) {
        str.push_back((char)code);
    } else if (code < 0x800) {
        str.push_back((char)(0xC0 | (code >> 6)));
        str.push_back((char)(0x80 | (code & 0x3F)));
    } else if (code < 0x10000) {
        str.push_back((char)(0xE0 | (code >> 12)));
        str.push_back((char)(0x80 | ((code >> 6) & 0x3F)));
        str.push_back((char)(0x80 | (code & 0x3F)));
    } else {
        str.push_back((char)(0xF0 | (code >> 18)));
        str.push_back((char)(0x80 | ((code >> 12) & 0x3F)));
        str.push_back((char)(0x80 | ((code >> 6) & 0x3F)));
        str.push_back((char)(0x80 | (code & 0x3F)));
    }
}

template <typename StreamType>
struct Parser
{
  public:
    Parser(const StreamType& stream) : m_it{stream} {}
    Parser(const Options& options, const StreamType& stream) : m_options{options}, m_it{stream} {}

    Parser(Parser&&) =  default;
    Parser(const Parser&) = delete;
    auto operator = (Parser&&) = delete;
    auto operator = (const Parser&) = delete;

    Object::ReprIX parse_type();  // quickly determine type without full parse
    bool parse();
    bool parse_any();
    bool parse_number();
    bool parse_string();
    bool parse_escape(std::string& str);
    bool parse_hex4(uint32_t& code);
    bool parse_map();
    bool parse_list();

    template <typename T>
    bool expect(const char* seq, T value);

    void consume_whitespace();

    Object::ReprIX get_map_type() const;
    void create_error(const std::string& message);

    Options m_options;
    StreamType m_it;
    Object m_curr;
    std::string m_scratch{32};
};

template <typename StreamType>
Object::ReprIX Parser<StreamType>::parse_type() {
    consume_whitespace();
    switch (m_it.peek()) {
        case '{': return get_map_type();
        case '[': return Object::LIST;
        case 'n': return Object::NIL;
        case 't':
        case 'f': return Object::BOOL;
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
            return Object::STR;
        default:
            break;
    }
    return Object::ERROR;
}

template <typename StreamType>
bool Parser<StreamType>::parse()
{
    m_curr = nil;
    if (!parse_any()) {
        if (m_curr == nil) m_curr = make_error("No object in json stream");
        return false;
    }
    return true;
}

template <typename StreamType>
bool Parser<StreamType>::parse_any()
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
            case 'n': return expect("null", nil);

            default:
                return false;
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
    errno = 0;
    if (is_float) {
        m_curr.refer_to(Object{strtod(str, &end)});
    } else {
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
    std::string str;
    for(; !m_it.done(); m_it.next()) {
        char c = m_it.peek();
        if (c == quote) {
            m_it.next();
            m_curr.refer_to(std::move(str));
            return true;
        } else if (c == '\\') {
            m_it.next();
            if (m_it.done()) break;
            if (!parse_escape(str)) return false;
        } else {
            str.push_back(c);
        }
    }

    create_error("Unterminated string");
    return false;
}

/// Decodes the escape sequence whose first character (after the backslash) is
/// the current character, leaving the stream on the last character of the sequence.
template <typename StreamType>
bool Parser<StreamType>::parse_escape(std::string& str) {
    switch (m_it.peek()) {
        case '"':  str.push_back('"'); break;
        case '\'': str.push_back('\''); break;  // single-quoted strings are supported
        case '\\': str.push_back('\\'); break;
        case '/':  str.push_back('/'); break;
        case 'b':  str.push_back('\b'); break;
        case 'f':  str.push_back('\f'); break;
        case 'n':  str.push_back('\n'); break;
        case 'r':  str.push_back('\r'); break;
        case 't':  str.push_back('\t'); break;
        case 'u': {
            uint32_t code;
            if (!parse_hex4(code)) return false;
            if (code >= 0xD800 && code <= 0xDBFF) {
                // high surrogate must be followed by an escaped low surrogate
                uint32_t low;
                m_it.next();
                if (m_it.done() || m_it.peek() != '\\') { create_error("Unpaired surrogate"); return false; }
                m_it.next();
                if (m_it.done() || m_it.peek() != 'u') { create_error("Unpaired surrogate"); return false; }
                if (!parse_hex4(low)) return false;
                if (low < 0xDC00 || low > 0xDFFF) { create_error("Unpaired surrogate"); return false; }
                code = 0x10000 + ((code - 0xD800) << 10) + (low - 0xDC00);
            } else if (code >= 0xDC00 && code <= 0xDFFF) {
                create_error("Unpaired surrogate");
                return false;
            }
            append_utf8(str, code);
            break;
        }
        default:
            create_error("Invalid escape sequence");
            return false;
    }
    return true;
}

/// Reads the 4 hex digits following the current character, leaving the stream on the last digit.
template <typename StreamType>
bool Parser<StreamType>::parse_hex4(uint32_t& code) {
    code = 0;
    for (int i = 0; i < 4; ++i) {
        m_it.next();
        if (m_it.done()) {
            create_error("Unterminated string");
            return false;
        }
        char c = m_it.peek();
        code <<= 4;
        if (c >= '0' && c <= '9')      code |= c - '0';
        else if (c >= 'a' && c <= 'f') code |= c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') code |= c - 'A' + 10;
        else {
            create_error("Invalid \\u escape");
            return false;
        }
    }
    return true;
}

template <typename StreamType>
bool Parser<StreamType>::parse_list() {
    ObjectList list;
    m_it.next();  // consume [
    consume_whitespace();
    if (m_it.peek() == ']') {
        m_it.next();
        m_curr.refer_to(std::move(list));
        return true;
    }
    while (!m_it.done()) {
        if (!parse_any()) {
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
    Object map = get_map_type();

    m_it.next();  // consume {
    consume_whitespace();
    if (m_it.peek() == '}') {
        m_it.next();
        m_curr.refer_to(std::move(map));
        return true;
    }

    while (!m_it.done()) {
        // key
        if (!parse_any()) {
            create_error("Expected dictionary key");
            return false;
        }

        Key key = m_curr.to_key();

        if (nodel::is_container(m_curr)) {
            create_error("Keys must be a primitive type");
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
        if (!parse_any()) {
            create_error("Expected dictionary value or object");
            return false;
        }

        map.set(key, m_curr);
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
    while (!m_it.done() && std::isspace(static_cast<unsigned char>(m_it.peek()))) m_it.next();
}

template <typename StreamType>
Object::ReprIX Parser<StreamType>::get_map_type() const {
    return m_options.use_sorted_map? Object::SMAP: Object::OMAP;
}

template <typename StreamType>
void Parser<StreamType>::create_error(const std::string& message)
{
    std::stringstream ss;
    ss << "JSON parse error at " << m_it.consumed() << ": " << message;
    m_curr = make_error(ss.str());
}

} // namespace impl


inline
Object parse(const Options& options, const std::string_view& str) {
    impl::Parser parser{options, nodel::parse::StringStreamAdapter{str}};
    parser.parse();
    return parser.m_curr;
}

inline
Object parse(const std::string_view& str) {
    return parse({}, str);
}

inline
Object parse_file(const Options& options, const std::string& file_name) {
    std::ifstream f_in{file_name, std::ios::in};
    if (!f_in.is_open()) {
        std::stringstream ss;
        ss << "Error opening file: " << file_name;
        return make_error(ss.str());
    } else {
        impl::Parser parser{options, nodel::parse::StreamAdapter{f_in}};
        parser.parse();
        return parser.m_curr;
    }
}

inline
Object parse_file(const std::string& file_name) {
    return parse_file({}, file_name);
}

} // namespace json


#ifndef NODEL_NO_JSON_LITERAL

inline
Object operator ""_json (const char* str, size_t size) {
    return json::parse({str, size});
}

#endif

} // namespace nodel
