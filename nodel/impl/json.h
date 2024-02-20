#pragma once

#include <ctype.h>
#include <cstdlib>
#include <cerrno>
#include <sstream>
#include <fstream>

#include <fmt/core.h>

#include "Object.h"
#include "Stopwatch.h"

namespace nodel {
namespace json {

constexpr bool log_stats = false;

class JsonException : public std::exception
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
  private:
    struct StreamIterator
    {
        StreamIterator(StreamType& stream) : stream{stream} {
            if (!stream.eof())
                fill();
        }

        StreamIterator(StreamIterator&&) = default;
        StreamIterator(const StreamIterator&) = delete;
        auto operator = (StreamIterator&&) = delete;
        auto operator = (StreamIterator&) = delete;

        char peek() { return buf[buf_pos]; }

        void next() {
            if (++buf_pos == buf_size) {
                if (stream.eof()) {
                    buf_pos--;
                    return;
                }
                fill();
            }
        }

        size_t consumed() const { return pos + buf_pos; }
        bool done() const { return buf_pos == buf_size; }
        bool error() const { return stream.fail(); }

        void fill() {
            pos += buf_size;
            stream.read(buf.data(), buf.size());
            buf_size = stream.gcount();
            buf_pos = 0;
            if (buf_size < buf.size())
                buf[buf_size++] = 0;
        }

        StreamType& stream;
        size_t pos = 0;
        std::array<char, 4096> buf;
        size_t buf_pos = 0;
        size_t buf_size = 0;
    };

  public:
    Parser(StreamType& stream) : it{stream}, swatch{"parse-json", false} {}

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

    StreamIterator it;
    Object curr;
    std::string scratch{32};
    int error_offset = 0;
    std::string error_message;
    debug::Stopwatch swatch;
};

template <typename StreamType>
Object::ReprType Parser<StreamType>::parse_type() {
    consume_whitespace();
    switch (it.peek()) {
        case '{': return Object::MAP_I;
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
                return curr.type();
            break;
        case '"':
        case '\'':
            return Object::STR_I;
        default:
            break;
    }
    return Object::BAD_I;
}

template <typename StreamType>
bool Parser<StreamType>::parse_object()
{
    if constexpr (log_stats) swatch.start();

    if (!parse_object('\0')) {
        if (error_message.size() == 0) {
            error_message = "No object in json stream";
        }
        return false;
    }

    if constexpr (log_stats) {
        swatch.stop();
        fmt::print("{:.3f} MB/s\n", it.consumed() / swatch.last() / 1000.0);
    }

    return true;
}

template <typename StreamType>
bool Parser<StreamType>::parse_object(char term_char)
{
    for (; !it.done(); it.next()) {
        switch (it.peek())
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
            case 'n': return expect("null", (void*)0);

            default:
                return it.peek() == term_char;
        }
    }
    return false;
}

template <typename StreamType>
bool Parser<StreamType>::parse_number() {
    scratch.clear();

    bool is_done = false;
    bool is_float = false;
    for (; !it.done(); it.next()) {
        char c = it.peek();
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
        scratch.push_back(c);
    }

    const char* str = scratch.c_str();
    const char* scratch_end = str + scratch.size();
    char* end = 0;
    if (is_float) {
        curr.empty();
        curr = Object{strtod(str, &end)};
    } else {
        curr.empty();
        curr = Object{strtoll(str, &end, 10)};
        if (errno) {
            errno = 0;
            curr.empty();
            curr = Object{strtoull(str, &end, 10)};
        }
    }

    scratch.clear();

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
    char quote = it.peek();
    it.next();
    bool escape = false;
    std::string str;
    for(; !it.done(); it.next()) {
        char c = it.peek();
        if (c == '\\') {
            escape = true;
        } else if (!escape) {
            if (c == quote) { it.next(); break; }
            str.push_back(c);
        } else {
            escape = false;
            str.push_back(c);
        }
    }

    if (it.done()) {
        create_error("Unterminated string");
        return false;
    }

    curr.empty();
    curr = std::move(str);
    return true;
}

template <typename StreamType>
bool Parser<StreamType>::parse_list() {
    List list;
    it.next();  // consume [
    consume_whitespace();
    if (it.peek() == ']') {
        it.next();
        curr.empty();
        curr = std::move(list);
        return true;
    }
    while (!it.done()) {
        if (!parse_object(']')) {
            create_error("Expected value or object");
            return false;
        }
        list.push_back(curr);
        consume_whitespace();
        char c = it.peek();
        if (c == ']') {
            it.next();
            curr.empty();
            curr = std::move(list);
            return true;
        } else if (c == ',') {
            it.next();
            continue;
        }
    }

    create_error("Unterminated list");
    return false;
}

template <typename StreamType>
bool Parser<StreamType>::parse_map() {
    Map map;
    it.next();  // consume {
    consume_whitespace();
    if (it.peek() == '}') {
        it.next();
        curr.empty();
        curr = std::move(map);
        return true;
    }

    while (!it.done()) {
        // key
        if (!parse_object(':')) {
            create_error("Expected dictionary key");
            return false;
        }

        Key key = curr.swap_key();

        if (curr.is_container()) {
            create_error("Map keys must be a primitive type");
            return false;
        }

        consume_whitespace();
        char c = it.peek();
        if (c != ':') {
            create_error("Expected token ':'");
            return false;
        }

        // consume :
        it.next();

        // value
        if (!parse_object('}')) {
            create_error("Expected dictionary value or object");
            return false;
        }

        map.emplace(key, curr);
        consume_whitespace();

        c = it.peek();
        if (c == '}') {
            it.next();
            curr.empty();
            curr = std::move(map);
            return true;
        } else if (c == ',') {
            it.next();
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
    for (; *seq_it != 0 && !it.done(); it.next(), seq_it++) {
        if (*seq_it != it.peek()) {
            create_error("Invalid literal");
            return false;
        }
    }
    curr.empty();
    curr = value;
    return true;
}

template <typename StreamType>
void Parser<StreamType>::consume_whitespace()
{
    while (!it.done() && std::isspace(it.peek())) it.next();
}

template <typename StreamType>
void Parser<StreamType>::create_error(const std::string& message)
{
    error_message = message;
    error_offset = it.consumed();
}

} // namespace impl


struct ParseError
{
    int error_offset = 0;
    std::string error_message;

    std::string to_str() const {
        if (error_message.size() > 0)
            return fmt::format("JSON parse error at {}: {}", error_offset, error_message);
        return "";
    }
};


inline
Object parse(std::string&& json, std::optional<ParseError>& error) {
    std::istringstream in{std::forward<std::string>(json)};
    impl::Parser parser{in};
    if (!parser.parse_object()) {
        error = ParseError{parser.error_offset, std::move(parser.error_message)};
        return null;
    }
    return parser.curr;
}

inline
Object parse(std::string&& json, std::string& error) {
    std::optional<nodel::json::ParseError> parse_error;
    Object result = nodel::json::parse(std::forward<std::string>(json), parse_error);
    if (parse_error) {
        error = parse_error->to_str();
        return null;
    }
    return result;
}

inline
Object parse(std::string&& json) {
    std::istringstream in{std::forward<std::string>(json)};
    impl::Parser parser{in};
    if (!parser.parse_object()) {
        return null;
    }
    return parser.curr;
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
        if (!parser.parse_object()) {
            ParseError parse_error{parser.error_offset, std::move(parser.error_message)};
            error = parse_error.to_str();
            return null;
        }
        return parser.curr;
    }
}

}} // namespace nodel.json
